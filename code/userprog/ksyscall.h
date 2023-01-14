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

int SysCreate(char *filename,int size)
{
	// return value
	// 1: success
	// 0: failed
	return kernel->fileSystem->Create(filename,size);
}

int SysWrite(char *buffer, int size, int id){
	return kernel->fileSystem->curopen->Write(buffer,size);
}
//When you finish the function "OpenAFile", you can remove the comment below.
OpenFileId SysOpen(char *name)
{
	if(kernel->fileSystem->Open(name)!=NULL)
        return 1;
	else return 0;
}

int SysRead(char *buffer, int size, int id){
	return kernel->fileSystem->curopen->Read(buffer,size);
}

int SysClose(int id)
{
	delete kernel->fileSystem->curopen;
	kernel->fileSystem->curopen = NULL;
    return 1;
}


#endif /* ! __USERPROG_KSYSCALL_H__ */
