// filesys.h
//	Data structures to represent the Nachos file system.
//
//	A file system is a set of files stored on disk, organized
//	into directories.  Operations on the file system have to
//	do with "naming" -- creating, opening, and deleting files,
//	given a textual file name.  Operations on an individual
//	"open" file (read, write, close) are to be found in the OpenFile
//	class (openfile.h).
//
//	We define two separate implementations of the file system.
//	The "STUB" version just re-defines the Nachos file system
//	operations as operations on the native UNIX file system on the machine
//	running the Nachos simulation.
//
//	The other version is a "real" file system, built on top of
//	a disk simulator.  The disk is simulated using the native UNIX
//	file system (in a file named "DISK").
//
//	In the "real" implementation, there are two key data structures used
//	in the file system.  There is a single "root" directory, listing
//	all of the files in the file system; unlike UNIX, the baseline
//	system does not provide a hierarchical directory structure.
//	In addition, there is a bitmap for allocating
//	disk sectors.  Both the root directory and the bitmap are themselves
//	stored as files in the Nachos file system -- this causes an interesting
//	bootstrap problem when the simulated disk is initialized.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation
// of liability and disclaimer of warranty provisions.

#ifndef FS_H
#define FS_H

// Sectors containing the file headers for the bitmap of free sectors,
// and the directory of files.  These file headers are placed in well-known
// sectors, so that they can be located on boot-up.
#define FreeMapSector 0
#define DirectorySector 1

// Initial file sizes for the bitmap and directory; until the file system
// supports extensible files, the directory size sets the maximum number
// of files that can be loaded onto the disk.
#define FreeMapFileSize (NumSectors / BitsInByte)
#define NumDirEntries 10
#define DirectoryFileSize (sizeof(DirectoryEntry) * NumDirEntries)

#include "copyright.h"
#include "sysdep.h"
#include "openfile.h"

#include "debug.h"
#include "disk.h"
#include "pbitmap.h"
#include "directory.h"
#include "filehdr.h"
#include <sstream>

typedef int OpenFileId;

#ifdef FILESYS_STUB // Temporarily implement file system calls as
// calls to UNIX, until the real file system
// implementation is available
class FileSystem
{
public:
	FileSystem()
	{
		for (int i = 0; i < 20; i++)
			fileDescriptorTable[i] = NULL;
	}

	bool Create(char *name)
	{
		int fileDescriptor = OpenForWrite(name);

		if (fileDescriptor == -1)
			return FALSE;
		Close(fileDescriptor);
		return TRUE;
	}

	OpenFile *Open(char *name)
	{
		int fileDescriptor = OpenForReadWrite(name, FALSE);

		if (fileDescriptor == -1)
			return NULL;
		return new OpenFile(fileDescriptor);
	}

	bool Remove(char *name) { return Unlink(name) == 0; }

	OpenFile *fileDescriptorTable[20];
};

#else // FILESYS
class FileSystem
{
public:
	FileSystem(bool format){
		DEBUG(dbgFile, "Initializing the file system.");
		if (format)
		{
			PersistentBitmap *freeMap = new PersistentBitmap(NumSectors);
			Directory *directory = new Directory(NumDirEntries);
			FileHeader *mapHdr = new FileHeader;
			FileHeader *dirHdr = new FileHeader;

			DEBUG(dbgFile, "Formatting the file system.");

			// First, allocate space for FileHeaders for the directory and bitmap
			// (make sure no one else grabs these!)
			freeMap->Mark(FreeMapSector);
			freeMap->Mark(DirectorySector);

			// Second, allocate space for the data blocks containing the contents
			// of the directory and bitmap files.  There better be enough space!

			ASSERT(mapHdr->Allocate(freeMap, FreeMapFileSize));
			ASSERT(dirHdr->Allocate(freeMap, DirectoryFileSize));

			// Flush the bitmap and directory FileHeaders back to disk
			// We need to do this before we can "Open" the file, since open
			// reads the file header off of disk (and currently the disk has garbage
			// on it!).

			DEBUG(dbgFile, "Writing headers back to disk.");
			mapHdr->WriteBack(FreeMapSector);
			dirHdr->WriteBack(DirectorySector);

			// OK to open the bitmap and directory files now
			// The file system operations assume these two files are left open
			// while Nachos is running.

			freeMapFile = new OpenFile(FreeMapSector);
			directoryFile = new OpenFile(DirectorySector);

			// Once we have the files "open", we can write the initial version
			// of each file back to disk.  The directory at this point is completely
			// empty; but the bitmap has been changed to reflect the fact that
			// sectors on the disk have been allocated for the file headers and
			// to hold the file data for the directory and bitmap.

			DEBUG(dbgFile, "Writing bitmap and directory back to disk.");
			freeMap->WriteBack(freeMapFile); // flush changes to disk
			directory->WriteBack(directoryFile);

			curOpenFile = NULL;

			if (debug->IsEnabled('f'))
			{
				freeMap->Print();
				directory->Print();
			}
			delete freeMap;
			delete directory;
			delete mapHdr;
			delete dirHdr;
		}
		else
		{
			// if we are not formatting the disk, just open the files representing
			// the bitmap and directory; these are left open while Nachos is running
			freeMapFile = new OpenFile(FreeMapSector);
			directoryFile = new OpenFile(DirectorySector);
		}
	}
	// MP4 mod tag
	~FileSystem(){
		delete freeMapFile;
		delete directoryFile;
	}

	bool Create(char *name, int initialSize){
		Directory *directory;
		PersistentBitmap *freeMap;
		FileHeader *hdr;
		int sector;
		bool success;

		DEBUG(dbgFile, "Creating file " << name << " size " << initialSize);

		directory = new Directory(NumDirEntries);
		directory->FetchFrom(directoryFile);

		OpenFile* curr_dir = directoryFile;
		string name_str(name);
		stringstream ss(name_str);
		char* name_c;
		while(getline(ss, name_str, '/'))
		{
			name_c = (char*)name_str.c_str();
			sector = directory->Find(name_c);
			if(sector == -1)
				break;
			else
			{
				if(!directory->isDir(name_c)){
					success = FALSE;
					break;
				}
				curr_dir = new OpenFile(sector);
				directory->FetchFrom(curr_dir);
			}
		}
		
		freeMap = new PersistentBitmap(freeMapFile, NumSectors);
		sector = freeMap->FindAndSet(); // find a sector to hold the file header
		if (sector == -1)
			success = FALSE; // no free block for file header
		else if (!directory->Add(name_c, sector, false))
			success = FALSE; // no space in directory
		else
		{
			hdr = new FileHeader;
			if (!hdr->Allocate(freeMap, initialSize))
				success = FALSE; // no space on disk for data
			else
			{
				success = TRUE;
				// everthing worked, flush all changes back to disk
				hdr->WriteBack(sector);
				directory->WriteBack(curr_dir);
				freeMap->WriteBack(freeMapFile);
			}
			delete hdr;
		}
		delete freeMap;
		
		delete directory;
		return success;
	}

	bool CreateDirectory(char *name){
		Directory *directory;
		PersistentBitmap *freeMap;
		FileHeader *hdr;
		int sector;
		bool success;

		DEBUG(dbgFile, "Creating directory " << name);

		directory = new Directory(NumDirEntries);
		directory->FetchFrom(directoryFile);

		OpenFile* curr_dir = directoryFile;
		string name_str(name);
		stringstream ss(name_str);
		char* name_c;
		while(getline(ss, name_str, '/'))
		{
			name_c = (char*)name_str.c_str();
			sector = directory->Find(name_c);
			if(sector == -1)
			{
				break;
			}
			else
			{
				if(!directory->isDir(name_c))
				{
					success = FALSE;
					break;
				}
				curr_dir = new OpenFile(sector);
				directory->FetchFrom(curr_dir);
			}
		}
		
		freeMap = new PersistentBitmap(freeMapFile, NumSectors);
		sector = freeMap->FindAndSet(); // find a sector to hold the file header
		if (sector == -1)
			success = FALSE; // no free block for file header
		else if (!directory->Add(name_c, sector, true))
			success = FALSE; // no space in directory
		else
		{
			hdr = new FileHeader;
			if (!hdr->Allocate(freeMap, DirectoryFileSize))
				success = FALSE; // no space on disk for data
			else
			{
				success = TRUE;
				// everthing worked, flush all changes back to disk
				hdr->WriteBack(sector);
				directory->WriteBack(directoryFile);
				freeMap->WriteBack(freeMapFile);
			}
			delete hdr;
		}
		delete freeMap;
		
		delete directory;
		return success;
	}

	OpenFile *Open(char *name){
		Directory *directory = new Directory(NumDirEntries);
		OpenFile *openFile = NULL;
		int sector;

		DEBUG(dbgFile, "Opening file" << name);
		directory->FetchFrom(directoryFile);
		
		OpenFile* curr_dir = directoryFile;
		string name_str(name);
		stringstream ss(name_str);
		char* name_c;
		while(getline(ss, name_str, '/')){
			name_c = (char*)name_str.c_str();
			sector = directory->Find(name_c);
			if(sector == -1)
				break;
			else{
				if(!directory->isDir(name_c))
					break;
				curr_dir = new OpenFile(sector);
				directory->FetchFrom(curr_dir);
			}
		}

		if (sector >= 0)
			openFile = new OpenFile(sector); // name was found in directory
		delete directory;
		
		curOpenFile = openFile;

		return openFile; // return NULL if not found
	}
	bool Remove(char *name, bool recurRemove){
		Directory *directory;
		PersistentBitmap *freeMap;
		FileHeader *fileHdr;
		int sector;

		directory = new Directory(NumDirEntries);
		directory->FetchFrom(directoryFile);

		OpenFile *currDir = directoryFile, *prevDir = NULL;
		char *prevTok, *currTok;
		currTok = strtok(name, "/");
		
		while(currTok){
			sector = directory->Find(currTok);
			if(sector == -1 || !directory->isDir(currTok))
				break;
			prevDir = currDir;
			currDir = new OpenFile(sector);
			directory->FetchFrom(currDir);
			prevTok = currTok;
			currTok = strtok(NULL, "/");
		}

		if (sector == -1){
			delete directory;
			return FALSE; // file not found
		}
		currTok = (currTok == NULL) ? prevTok : currTok;
		freeMap = new PersistentBitmap(freeMapFile, NumSectors);

		if(recurRemove && directory->isDir(currTok)){
			directory->recurRemove(freeMap);
			directory->FetchFrom(prevDir);
			currDir = prevDir;
			currTok = prevTok;
		}
		
		fileHdr = new FileHeader;
		fileHdr->FetchFrom(sector);

		fileHdr->Deallocate(freeMap); // remove data blocks
		freeMap->Clear(sector);       // remove header block
		directory->Remove(currTok);

		freeMap->WriteBack(freeMapFile);     // flush to disk
		directory->WriteBack(currDir); // flush to disk
		delete fileHdr;
		delete directory;
		delete freeMap;
		return TRUE;
	}

	void List(char* name, bool recur_list){
		Directory *directory = new Directory(NumDirEntries);
		OpenFile* curr_dir = directoryFile;
		string name_str(name);
		stringstream ss(name_str);
		char* name_c;

		while(getline(ss, name_str, '/')){
			name_c = (char*)name_str.c_str();
			if(directory->Find(name_c) == -1 || !directory->isDir(name_c))
				break;
			curr_dir = new OpenFile(directory->Find(name_c));
			directory->FetchFrom(curr_dir);
		}
		
		recur_list? directory->RecurList(0): directory->List();
		delete directory;
	}

	void Print(){
		FileHeader *bitHdr = new FileHeader;
		FileHeader *dirHdr = new FileHeader;
		PersistentBitmap *freeMap = new PersistentBitmap(freeMapFile, NumSectors);
		Directory *directory = new Directory(NumDirEntries);

		printf("Bit map file header:\n");
		bitHdr->FetchFrom(FreeMapSector);
		bitHdr->Print();

		printf("Directory file header:\n");
		dirHdr->FetchFrom(DirectorySector);
		dirHdr->Print();

		freeMap->Print();

		directory->FetchFrom(directoryFile);
		directory->Print();

		delete bitHdr;
		delete dirHdr;
		delete freeMap;
		delete directory;
	}

	OpenFile* curOpenFile;

	int WriteFile(char* buffer, int size, OpenFileId id){
		return curOpenFile->Write(buffer, size);
	}

	int ReadFile(char* buffer, int size, OpenFileId id){
		return curOpenFile->Read(buffer, size);
	}

	void CloseFile(OpenFileId id){
		delete curOpenFile;
		curOpenFile = NULL;
	}

private:
	OpenFile *freeMapFile;	 // Bit map of free disk blocks,
							 // represented as a file
	OpenFile *directoryFile; // "Root" directory -- list of
							 // file names, represented as a file
};

#endif // FILESYS

#endif // FS_H
