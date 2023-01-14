//  filesys.cc
//	Routines to manage the overall operation of the file system.
//	Implements routines to map from textual file names to files.
//
//	Each file in the file system has:
//	   A file header, stored in a sector on disk
//		(the size of the file header data structure is arranged
//		to be precisely the size of 1 disk sector)
//	   A number of data blocks
//	   An entry in the file system directory
//
// 	The file system consists of several data structures:
//	   A bitmap of free disk sectors (cf. bitmap.h)
//	   A directory of file names and file headers
//
//      Both the bitmap and the directory are represented as normal
//	files.  Their file headers are located in specific sectors
//	(sector 0 and sector 1), so that the file system can find them
//	on bootup.
//
//	The file system assumes that the bitmap and directory files are
//	kept "open" continuously while Nachos is running.
//
//	For those operations (such as Create, Remove) that modify the
//	directory and/or bitmap, if the operation succeeds, the changes
//	are written immediately back to disk (the two files are kept
//	open during all this time).  If the operation fails, and we have
//	modified part of the directory and/or bitmap, we simply discard
//	the changed version, without writing it back to disk.
//
// 	Our implementation at this point has the following restrictions:
//
//	   there is no synchronization for concurrent accesses
//	   files have a fixed size, set when the file is created
//	   files cannot be bigger than about 3KB in size
//	   there is no hierarchical directory structure, and only a limited
//	     number of files can be added to the system
//	   there is no attempt to make the system robust to failures
//	    (if Nachos exits in the middle of an operation that modifies
//	    the file system, it may corrupt the disk)
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation
// of liability and disclaimer of warranty provisions.

/*
#ifndef FILESYS_STUB

#include "copyright.h"
#include "debug.h"
#include "disk.h"
#include "pbitmap.h"
#include "directory.h"
#include "filehdr.h"
#include "filesys.h"

// Sectors containing the file headers for the bitmap of free sectors,
// and the directory of files.  These file headers are placed in well-known
// sectors, so that they can be located on boot-up.
#define FreeMapSector 0
#define DirectorySector 1

// Initial file sizes for the bitmap and directory; until the file system
// supports extensible files, the directory size sets the maximum number
// of files that can be loaded onto the disk.
#define FreeMapFileSize (NumSectors / BitsInByte)
#define NumDirEntries 64
#define DirectoryFileSize (sizeof(DirectoryEntry) * NumDirEntries)

//----------------------------------------------------------------------
// FileSystem::FileSystem
// 	Initialize the file system.  If format = TRUE, the disk has
//	nothing on it, and we need to initialize the disk to contain
//	an empty directory, and a bitmap of free sectors (with almost but
//	not all of the sectors marked as free).
//
//	If format = FALSE, we just have to open the files
//	representing the bitmap and the directory.
//
//	"format" -- should we initialize the disk?
//----------------------------------------------------------------------

FileSystem::FileSystem(bool format)
{
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
        curopen = NULL;
        // Once we have the files "open", we can write the initial version
        // of each file back to disk.  The directory at this point is completely
        // empty; but the bitmap has been changed to reflect the fact that
        // sectors on the disk have been allocated for the file headers and
        // to hold the file data for the directory and bitmap.

        DEBUG(dbgFile, "Writing bitmap and directory back to disk.");
        freeMap->WriteBack(freeMapFile); // flush changes to disk
        directory->WriteBack(directoryFile);

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

//----------------------------------------------------------------------
// MP4 mod tag
// FileSystem::~FileSystem
//----------------------------------------------------------------------
FileSystem::~FileSystem()
{
    delete freeMapFile;
    delete directoryFile;
}

//----------------------------------------------------------------------
// FileSystem::Create
// 	Create a file in the Nachos file system (similar to UNIX create).
//	Since we can't increase the size of files dynamically, we have
//	to give Create the initial size of the file.
//
//	The steps to create a file are:
//	  Make sure the file doesn't already exist
//        Allocate a sector for the file header
// 	  Allocate space on disk for the data blocks for the file
//	  Add the name to the directory
//	  Store the new file header on disk
//	  Flush the changes to the bitmap and the directory back to disk
//
//	Return TRUE if everything goes ok, otherwise, return FALSE.
//
// 	Create fails if:
//   		file is already in directory
//	 	no free space for file header
//	 	no free entry for file in directory
//	 	no free space for data blocks for the file
//
// 	Note that this implementation assumes there is no concurrent access
//	to the file system!
//
//	"name" -- name of file to be created
//	"initialSize" -- size of file to be created
//----------------------------------------------------------------------

bool FileSystem::Create(char *name, int initialSize)
{
    Directory *directory;
    PersistentBitmap *freeMap;
    FileHeader *hdr;
    int sector;
    bool success;
    char *cur, *temp;
    DEBUG(dbgFile, "Creating file " << name << " size " << initialSize);
    directory = new Directory(NumDirEntries);
    directory->FetchFrom(directoryFile);
    // mp4 hw
    cur = strtok(name, "/");
    OpenFile* dirnow = directoryFile;
    while (cur != NULL) {
        sector = directory->Find(cur);
        if (sector != -1){
            if(!directory->IsDir(cur)){
                success = FALSE;
                break;
            }
            dirnow = new OpenFile(sector);	
            directory->FetchFrom(dirnow);
            cur = strtok(NULL, "/"); 
        }
        else{
            printf("%s\n", cur);
            break;
        }
        	   
    }

    if (directory->Find(cur) != -1)
        success = FALSE; // file is already in directory
    else
    {
        freeMap = new PersistentBitmap(freeMapFile, NumSectors);
        sector = freeMap->FindAndSet(); // find a sector to hold the file header
        if (sector == -1)
            success = FALSE; // no free block for file header
        else if (!directory->Add(cur, sector, false))
            success = FALSE; // no space in directory
        else{
            hdr = new FileHeader;
            if (!hdr->Allocate(freeMap, initialSize))
                success = FALSE; // no space on disk for data
            else{
                success = TRUE;
                // everthing worked, flush all changes back to disk
                hdr->WriteBack(sector);
                directory->WriteBack(dirnow);
                freeMap->WriteBack(freeMapFile);
                //mp4 PART II(3) test
                if (debug->IsEnabled('b'))
                {
                    printf("==========mp4 PART II(3) test===========\n");
                    printf("file size: %d", hdr->FileLength());
                    printf("  file header num: %d", hdr->getFileHeaderNum(hdr->FileLength()));
                    printf("  total file head size: %d\n", hdr->getFileHeaderNum(hdr->FileLength())*SectorSize);
                }
            }
            delete hdr;
        }
        delete freeMap;
    }
    delete directory;
    return success;
}

//----------------------------------------------------------------------
// FileSystem::Open
// 	Open a file for reading and writing.
//	To open a file:
//	  Find the location of the file's header, using the directory
//	  Bring the header into memory
//
//	"name" -- the text name of the file to be opened
//----------------------------------------------------------------------
//mp4
OpenFile * FileSystem::Open(char *name)
{
    Directory *directory = new Directory(NumDirEntries);
    OpenFile *openFile = NULL;
    int sector;
    directory->FetchFrom(directoryFile); 
     // mp4 hw
    char *cur, *temp;
    std::cout<<"name: "<<name<<std::endl;
    temp = strtok(name, "/");
    cur = temp;
    while (cur != NULL) {
        sector = directory->Find(cur);
        if (directory->IsDir(cur)){
            openFile = new OpenFile(sector);	
            directory->FetchFrom(openFile);
		std::cout<<"is dir open: "<<cur<<std::endl;
            cur = strtok(NULL, "/"); 
        }
        else{
		std::cout<<"not dir open: "<<cur<<std::endl;
            break;
        }
        	   
    }
    DEBUG(dbgFile, "Opening file" << cur);
    if (sector >= 0)
        openFile = new OpenFile(sector); // name was found in directory
    delete directory;
    //mp4
    curopen = openFile;
    return openFile; // return NULL if not found
}

//mp4
bool FileSystem::CreateDirectory(char *name){
    Directory *directory= new Directory(NumDirEntries);
    PersistentBitmap *freeMap;
    FileHeader *hdr;
    int sector;
    bool success;

    DEBUG(dbgFile, "Creating directory " << name );

    directory->FetchFrom(directoryFile);
    
    char *temp,*cur;
    temp = strtok(name, "/");
    cur = temp;
    OpenFile* dirnow = directoryFile;
    while (cur != NULL) {
        sector = directory->Find(cur);
        if (sector != -1){
            dirnow = new OpenFile(sector);	
            directory->FetchFrom(dirnow);
            cur = strtok(NULL, "/"); 
        }
        else{
            printf("%s\n", cur);
            break;
        }
        	   
    }

    ASSERT(directory->Find(cur) == -1)
    freeMap = new PersistentBitmap(freeMapFile, NumSectors);
    sector = freeMap->FindAndSet(); // find a sector to hold the file header
    ASSERT(sector != -1);
    ASSERT(directory->Add(cur, sector, true))
    hdr = new FileHeader;
    ASSERT(hdr->Allocate(freeMap,DirectoryFileSize))
    hdr->WriteBack(sector);
    OpenFile *newopen = new OpenFile(sector);
    Directory *newDir = new Directory(NumDirEntries);
    newDir->WriteBack(newopen);
    directory->WriteBack(dirnow);
    freeMap->WriteBack(freeMapFile);
    delete hdr;
    delete freeMap;
    delete directory;
    delete newDir;
    delete newopen;
    success = true;
    return success;
}

//----------------------------------------------------------------------
// FileSystem::Remove
// 	Delete a file from the file system.  This requires:
//	    Remove it from the directory
//	    Delete the space for its header
//	    Delete the space for its data blocks
//	    Write changes to directory, bitmap back to disk
//
//	Return TRUE if the file was deleted, FALSE if the file wasn't
//	in the file system.
//
//	"name" -- the text name of the file to be removed
//----------------------------------------------------------------------
bool FileSystem::Remove(char *name, bool recursive)
{
    Directory *directory;
    PersistentBitmap *freeMap;
    FileHeader *fileHdr;
    int sector;

    directory = new Directory(NumDirEntries);
    directory->FetchFrom(directoryFile);
  
    char *cur, *prev;
    cur = strtok(name, "/");
    OpenFile* dirnow = directoryFile;
    OpenFile* prevdir;

    while (cur != NULL) {
        sector = directory->Find(cur);
        if (sector == -1 || !directory->IsDir(cur)){
            break;
        }
        else{
	    prevdir = dirnow;
            dirnow = new OpenFile(sector);	
            directory->FetchFrom(dirnow);
	    prev = cur;
            cur = strtok(NULL, "/"); 
        }
        	   
    }
    if (sector == -1) // file not found
    {
        delete directory;
        return FALSE; 
    }
    if(cur == NULL)cur = prev;
    freeMap = new PersistentBitmap(freeMapFile, NumSectors);
    if(recursive && directory->IsDir(cur)){
        directory->RecursiveRemove(freeMap);
	    directory->FetchFrom(prevdir);
	    dirnow = prevdir; //本身還沒刪
        cur = prev;	 
    }
    //刪file或是刪recursive本身
    fileHdr = new FileHeader;  
    fileHdr->FetchFrom(sector);
    fileHdr->Deallocate(freeMap); // remove data blocks
    freeMap->Clear(sector); // remove data blocks
    directory->Remove(cur);

    cout<<"cur remove "<<cur<<endl;
    freeMap->WriteBack(freeMapFile);     // flush to disk
    directory->WriteBack(dirnow);
//    directory->WriteBack(directoryFile); // flush to disk
    delete fileHdr;
    delete directory;
    delete freeMap;
    return TRUE;

}




//----------------------------------------------------------------------
// FileSystem::List
// 	List all the files in the file system directory.
//----------------------------------------------------------------------
void FileSystem::List(char* name)
{
    Directory *directory = new Directory(NumDirEntries);
    directory->FetchFrom(directoryFile);
    char *cur;
    cur = strtok(name, "/");
    OpenFile* dirnow = directoryFile;
    int sec;
    while (cur != NULL) {
	std::cout<<"list "<<cur<<std::endl;
        sec = directory->Find(cur);
        if(sec == -1 || !directory->IsDir(cur))break;
        else{
            dirnow = new OpenFile(sec);	
            directory->FetchFrom(dirnow);
            cur = strtok(NULL, "/"); 
        }
        	   
    }

    directory->List();
    delete directory;
}

void FileSystem::RecursiveList(char* name)
{
    Directory *directory = new Directory(NumDirEntries);
    directory->FetchFrom(directoryFile);
    char *cur;
    cur = strtok(name, "/");
    OpenFile* dirnow = directoryFile;
    int sec;
    while (cur != NULL) {
	cout<<"recursive "<<cur<<endl;
        sec = directory->Find(cur);
        if(sec == -1 || !directory->IsDir(cur))break; // file not found or is just a file
        else{
            dirnow = new OpenFile(sec);	
            directory->FetchFrom(dirnow);
            cur = strtok(NULL, "/"); 
        }
        	   
    }

    directory->RecursiveList(0); // num means 往內縮排多少格
    delete directory;
    if(cur != NULL){
        delete dirnow;
    }
}

//----------------------------------------------------------------------
// FileSystem::Print
// 	Print everything about the file system:
//	  the contents of the bitmap
//	  the contents of the directory
//	  for each file in the directory,
//	      the contents of the file header
//	      the data in the file
//----------------------------------------------------------------------
void FileSystem::Print()
{
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

#endif // FILESYS_STUB

*
/*
#ifndef FILESYS_STUB

#include "copyright.h"
#include "debug.h"
#include "disk.h"
#include "pbitmap.h"
#include "directory.h"
#include "filehdr.h"
#include "filesys.h"
#include <sstream>

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

//----------------------------------------------------------------------
// FileSystem::FileSystem
// 	Initialize the file system.  If format = TRUE, the disk has
//	nothing on it, and we need to initialize the disk to contain
//	an empty directory, and a bitmap of free sectors (with almost but
//	not all of the sectors marked as free).
//
//	If format = FALSE, we just have to open the files
//	representing the bitmap and the directory.
//
//	"format" -- should we initialize the disk?
//----------------------------------------------------------------------

FileSystem::FileSystem(bool format)
{
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

//----------------------------------------------------------------------
// MP4 mod tag
// FileSystem::~FileSystem
//----------------------------------------------------------------------
FileSystem::~FileSystem()
{
    delete freeMapFile;
    delete directoryFile;
}

//----------------------------------------------------------------------
// FileSystem::Create
// 	Create a file in the Nachos file system (similar to UNIX create).
//	Since we can't increase the size of files dynamically, we have
//	to give Create the initial size of the file.
//
//	The steps to create a file are:
//	  Make sure the file doesn't already exist
//        Allocate a sector for the file header
// 	  Allocate space on disk for the data blocks for the file
//	  Add the name to the directory
//	  Store the new file header on disk
//	  Flush the changes to the bitmap and the directory back to disk
//
//	Return TRUE if everything goes ok, otherwise, return FALSE.
//
// 	Create fails if:
//   		file is already in directory
//	 	no free space for file header
//	 	no free entry for file in directory
//	 	no free space for data blocks for the file
//
// 	Note that this implementation assumes there is no concurrent access
//	to the file system!
//
//	"name" -- name of file to be created
//	"initialSize" -- size of file to be created
//----------------------------------------------------------------------

bool FileSystem::Create(char *name, int initialSize)
{
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

int FileSystem::WriteFile(char* buffer, int size, OpenFileId id){
    return curOpenFile->Write(buffer, size);
}

int FileSystem::ReadFile(char* buffer, int size, OpenFileId id){
    return curOpenFile->Read(buffer, size);
}

// void FileSystem::CloseFile(OpenFileId id);


bool FileSystem::CreateDirectory(char *name)
{
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

//----------------------------------------------------------------------
// FileSystem::Open
// 	Open a file for reading and writing.
//	To open a file:
//	  Find the location of the file's header, using the directory
//	  Bring the header into memory
//
//	"name" -- the text name of the file to be opened
//----------------------------------------------------------------------

OpenFile * FileSystem::Open(char *name)
{
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

//----------------------------------------------------------------------
// FileSystem::Remove
// 	Delete a file from the file system.  This requires:
//	    Remove it from the directory
//	    Delete the space for its header
//	    Delete the space for its data blocks
//	    Write changes to directory, bitmap back to disk
//
//	Return TRUE if the file was deleted, FALSE if the file wasn't
//	in the file system.
//
//	"name" -- the text name of the file to be removed
//----------------------------------------------------------------------

bool FileSystem::Remove(char *name, bool recurRemove)
{
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

//----------------------------------------------------------------------
// FileSystem::List
// 	List all the files in the file system directory.
//----------------------------------------------------------------------

void FileSystem::List(char* name, bool recur_list)
{
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

//----------------------------------------------------------------------
// FileSystem::Print
// 	Print everything about the file system:
//	  the contents of the bitmap
//	  the contents of the directory
//	  for each file in the directory,
//	      the contents of the file header
//	      the data in the file
//----------------------------------------------------------------------

void FileSystem::Print()
{
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

#endif // FILESYS_STUB
*/