// filesys.cc 
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

#include "copyright.h"

#include "disk.h"
#include "bitmap.h"
#include "directory.h"
#include "filehdr.h"
#include "filesys.h"
#include "system.h"

// Sectors containing the file headers for the bitmap of free sectors,
// and the directory of files.  These file headers are placed in well-known 
// sectors, so that they can be located on boot-up.
#define FreeMapSector 		0
#define DirectorySector 	1

// Initial file sizes for the bitmap and directory; until the file system
// supports extensible files, the directory size sets the maximum number 
// of files that can be loaded onto the disk.
#define FreeMapFileSize 	(NumSectors / BitsInByte)
#define NumDirEntries 		10
#define DirectoryFileSize 	(sizeof(DirectoryEntry) * NumDirEntries)
//We fetcht the directory from the currentThread
#define currentDirectory    currentThread->currentDirectory

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
    DEBUG('f', "Initializing the file system.\n");
    if (format) {
        BitMap *freeMap = new BitMap(NumSectors);
        Directory *directory = new Directory(NumDirEntries);
        FileHeader *mapHdr = new FileHeader;
        FileHeader *dirHdr = new FileHeader;

            DEBUG('f', "Formatting the file system.\n");     
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

            DEBUG('f', "Writing headers back to disk.\n");
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
        directory->Add(".", DirectorySector, TRUE);
        directory->Add("..", DirectorySector, TRUE);
        initializeOpenFileTable();
        DEBUG('f', "Writing bitmap and directory back to disk.\n");
        freeMap->WriteBack(freeMapFile);	 // flush changes to disk
        currentDirectory = directoryFile;
        directory->WriteBack(currentDirectory);
        if (DebugIsEnabled('f')) {
            freeMap->Print();
            directory->Print();

            delete freeMap; 
        delete directory; 
        delete mapHdr; 
        delete dirHdr;
        }
    } else {
    // if we are not formatting the disk, just open the files representing
    // the bitmap and directory; these are left open while Nachos is running	
		freeMapFile = new OpenFile(FreeMapSector);
        directoryFile = new OpenFile(DirectorySector);
        currentDirectory = directoryFile;
        if (openFileTable == NULL) 
            initializeOpenFileTable();
    }
}

//IFT320

void FileSystem::initializeOpenFileTable(){
    printf("Initializing open file table \n");
    openFileTable = new OpenedFile[NumDirEntries];
    for(int i = 0; i<NumDirEntries; i++){
        openFileTable[i].name = NULL;
        openFileTable[i].nbUses = 0;
        openFileTable[i].writing = FALSE;
        openFileTable[i].openFile = NULL;
    }
}

int FileSystem::nextOpenFileIndex(char *name){
    printf("NextOpenIndex for %s \n", name);
    int j = NumDirEntries;
    for(int i = 0; i<NumDirEntries; i++){
        if(openFileTable[i].name == name){
            return i;
        }
        if(openFileTable[i].openFile == NULL && i<j)
            j = i; //First free space
    }
    return j < NumDirEntries ? j : -1;
}

int FileSystem::Read(FileHandle file, char *into, int numBytes) {
    printf("Read file at index %i \n", file);	
    return openFileTable[file].openFile->Read(into, numBytes);
}

int FileSystem::Write(FileHandle file, char *from, int numBytes) {
    printf("Write file at index %i \n", file);	
    return openFileTable[file].openFile->Write(from, numBytes);
}

int FileSystem::ReadAt(FileHandle file, char *into, int numBytes,int position) {
    printf("ReadAt file at index %i \n", file);	
    return openFileTable[file].openFile->ReadAt(into, numBytes,position);
}

int FileSystem::WriteAt(FileHandle file, char *from, int numBytes,int position) {
    printf("WriteAt file at index %i \n", file);	
    return openFileTable[file].openFile->WriteAt(from,numBytes,position);
}


void FileSystem::Close (FileHandle file){
    printf("Close Opened File %i with %i uses\n", file, openFileTable[file].nbUses);
    openFileTable[file].nbUses--;
	if(0 >= openFileTable[file].nbUses){
        if(openFileTable[file].openFile != NULL){
           openFileTable[file].openFile = NULL;
        }
        if(openFileTable[file].name != NULL){
            openFileTable[file].name = NULL;
        }
        openFileTable[file].writing = false;
     }
    
}
void FileSystem::CloseAll(){
    printf("Close all Opened Files \n");
	for(int i = 0; i<NumDirEntries; i++){
        openFileTable[i].nbUses = 0;
        if(openFileTable[i].openFile != NULL){
            openFileTable[i].openFile = NULL;
        }
        if(openFileTable[i].name != NULL){
            openFileTable[i].name = NULL;
        }
        openFileTable[i].writing = false;
        
    }
}

void FileSystem::TouchOpenedFiles(char * modif){
    printf("Touch Opened Files \n");
	for(int i = 0; i<NumDirEntries; i++){
        if(openFileTable[i].nbUses > 0){
            openFileTable[i].writing = TRUE;
            openFileTable[i].openFile->Write(modif, strlen(modif));
            openFileTable[i].writing = FALSE;
        }
    }
}


//IFT320: Fonction de changement de repertoire. Doit etre implementee pour la partie A.
bool FileSystem::ChangeDirectory(char* name){
    
    printf("\n cd %s \n", name);

	//IFT320: Partie A
    //Find Sector with name
    OpenFile *openFile = NULL;
    bool didChangeDirectory = FALSE;
    int sector;	
    Directory *directory = new Directory(NumDirEntries);
    directory->FetchFrom(currentDirectory);
    //Will return a valid sector number if the file found is a Directory
    sector = directory->FindDirectory(name);
    if(sector != -1){
        //Open the directory
        openFile = new OpenFile(sector);
        if(openFile != NULL){
            delete currentDirectory;
            currentDirectory = openFile;
            didChangeDirectory = TRUE;
        } 	
    }
    delete directory;	
    return didChangeDirectory;
}


//IFT320: Fonction de creation de repertoires. Doit etre implementee pour la partie A.
bool FileSystem::CreateDirectory(char *name)
{
	//IFT320: Partie A
	BitMap *freeMap;
    FileHeader *hdr;
    int sector;

    bool directoryCreated = FALSE;

    printf("Creating directory...%s \n", name);

    //Fetch Current Directory
    Directory *directory = new Directory(NumDirEntries);
    directory->FetchFrom(currentDirectory);

    if(directory->Find(name) != -1){
        printf("A file/directory with the same name already exists in this directory.\n");
    }else{
        freeMap = new BitMap(NumSectors);
        freeMap->FetchFrom(freeMapFile);
        sector = freeMap->Find();

        if(sector != -1){
            if(!directory->isFull()){
                hdr = new FileHeader;
                if(hdr->Allocate(freeMap, DirectoryFileSize)){
                    directory->Add(name,sector,TRUE);
                    hdr->WriteBack(sector);
                    freeMap->WriteBack(freeMapFile);
                    OpenFile *openFile = new OpenFile(sector);
                    Directory *newDir = new Directory(NumDirEntries);
                    newDir->Add(".",sector,true);
                    int parentDir = directory->FindDirectory(".");
                    newDir->Add("..", parentDir, true);
                    newDir->WriteBack(openFile);
                    directory->WriteBack(currentDirectory);
                }else{
                    printf("No free block to accomodate file headerdr \n");
                }
                delete hdr;
            }else{
                printf("No free space in the Directory \n");
            }
        }else{
            printf("No free space \n");
        }
        delete freeMap;
    }

	delete directory;
    return directoryCreated;
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
    BitMap *freeMap;
    FileHeader *hdr;
    int sector;
    bool success;

    printf("Creating file %s, size %d\n", name, initialSize);

    directory = new Directory(NumDirEntries);
	directory->FetchFrom(currentDirectory);

    if (directory->Find(name) != -1){
      success = FALSE;			// file is already in directory
    }else {	
        freeMap = new BitMap(NumSectors);
        freeMap->FetchFrom(freeMapFile);
        sector = freeMap->Find();	// find a sector to hold the file header
    	if (sector == -1) 		
            success = FALSE;		// no free block for file header 
        else if (!directory->Add(name, sector, FALSE))
            success = FALSE;	// no space in directory
        else {
                hdr = new FileHeader;
            if (!hdr->Allocate(freeMap, initialSize))
                    success = FALSE;	// no space on disk for data
            else {	
                success = TRUE;
            // everthing worked, flush all changes back to disk
                    hdr->WriteBack(sector); 		
                    directory->WriteBack(currentDirectory);
                    freeMap->WriteBack(freeMapFile);
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

FileHandle FileSystem::Open(char *name)
{ 
    Directory *directory = new Directory(NumDirEntries);
    OpenFile *openFile = NULL;
    int sector;	
	directory->FetchFrom(currentDirectory);
    if(directory->isDirectory(name))
        ChangeDirectory(name);
    sector = directory->Find(name); 
    int fileIndex = -1;
    printf("Opening file : %s\n", name);
	if (sector >= 0){
        fileIndex = nextOpenFileIndex(name);
        printf("FileIndex: %d \n", fileIndex);
        if(fileIndex == -1){
            printf("Open file table full...");
        }else{
            if(openFileTable[fileIndex].openFile == NULL){
                openFile = new OpenFile(sector);
                openFileTable[fileIndex].name = name;
                openFileTable[fileIndex].openFile = openFile;
            }
            openFileTable[fileIndex].nbUses++;
        }
    }else{
        printf("Couldn't find file in current directory");
    }
    delete directory;
    return fileIndex;				// return NULL if not found
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

bool FileSystem::Remove(char *name)
{ 
	//IFT320: partie A
	printf("Removing %s \n", name);
    Directory *directory;
    BitMap *freeMap;
    FileHeader *fileHdr;
    int sector;
    directory = new Directory(NumDirEntries);
	
    directory->FetchFrom(currentDirectory);
    sector = directory->Find(name);
    if (sector == -1 || directory->isDirectory(name)) {
       delete directory;
       return FALSE;			 // file not found or isDirectory
    }
    int index = nextOpenFileIndex(name);
    if(index != -1){
        if(openFileTable[index].name == name)
            if(openFileTable[index].writing){
                printf("File is open for writing... \n");
                delete directory;
                return FALSE;
            }
                
    }
	fileHdr = new FileHeader;
    fileHdr->FetchFrom(sector);
	
    freeMap = new BitMap(NumSectors);
    freeMap->FetchFrom(freeMapFile);

    fileHdr->Deallocate(freeMap);  		// remove data blocks
    freeMap->Clear(sector);			// remove header block
    directory->Remove(name);

    freeMap->WriteBack(freeMapFile);		// flush to disk
    directory->WriteBack(currentDirectory);        // flush to disk
    delete fileHdr;
    delete directory;
    delete freeMap;
    return TRUE;
} 

//----------------------------------------------------------------------
// FileSystem::List
// 	List all the files in the file system directory.
//----------------------------------------------------------------------

void
FileSystem::List()
{
    Directory *directory = new Directory(NumDirEntries);
	
    directory->FetchFrom(currentDirectory);
    directory->List();
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

void
FileSystem::Print()
{
    FileHeader *bitHdr = new FileHeader;
    FileHeader *dirHdr = new FileHeader;
    BitMap *freeMap = new BitMap(NumSectors);
    Directory *directory = new Directory(NumDirEntries);

    printf("Bit map file header:\n");
    bitHdr->FetchFrom(FreeMapSector);
    bitHdr->Print();

    printf("Directory file header:\n");
    dirHdr->FetchFrom(DirectorySector);
    dirHdr->Print();

    freeMap->FetchFrom(freeMapFile);
    freeMap->Print();
	
    directory->FetchFrom(currentDirectory);
    directory->Print();

    delete bitHdr;
    delete dirHdr;
    delete freeMap;
    delete directory;
} 
