/**************************************************************
 *
 * userprog/ksyscall.h
 *
 * Kernel interface for systemcalls 
 *
 * by Marcus Voelp  (c) Universitaet Karlsruhe
 *
 **************************************************************/

#ifndef __USERPROG_KSYSCALL_H__
#define __USERPROG_KSYSCALL_H__

#include "kernel.h"

#include "synchconsole.h"

void SysHalt()
{
	kernel->interrupt->Halt();
}

int SysAdd(int op1, int op2)
{
	return op1 + op2;
}

#ifdef FILESYS_STUB
int SysCreate(char *filename)
{
	// return value
	// 1: success
	// 0: failed
	return kernel->interrupt->CreateFile(filename);
}
#endif

int SysCreate(char *name, int size){
	kernel->fileSystem->Create(name, size);
	return 1;
}

OpenFileId SysOpen(char* name){
	OpenFile *file = kernel->fileSystem->Open(name);
	if(file==nullptr){
		return 0;
	}
	return 1;
}

int SysRead(char *buf, int size, OpenFileId id){
	return kernel->fileSystem->ReadFile(buf, size, id);
}

int SysWrite(char *buf, int size, OpenFileId id){
	return kernel->fileSystem->WriteFile(buf, size, id);
}

int SysClose(OpenFileId id){
	kernel->fileSystem->CloseFile(id);
	return 1;
}

#endif /* ! __USERPROG_KSYSCALL_H__ */
