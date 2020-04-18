// ADFSCopy.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "IDE8Source.h"
#include "ImageDataSource.h"
#include "ADFSFileSystem.h"
#include "ADFSDirectory.h"

ADFSFileSystem *Source;
ADFSFileSystem *Destination;

int CopyObject(const char *Obj, FileSystem *Destination, FileSystem *Source) {

	std::vector<BBCFile>::iterator	iFile,dFile;

	int	cReference = 0;

	for (iFile = Source->pDirectory->Files.begin(); iFile != Source->pDirectory->Files.end(); iFile++) {
		if (strcmp((char *) iFile->filename, Obj) == 0) {
			printf("File %s\n", iFile->filename);
			if (iFile->Directory) {
				Destination->CreateDirectory(&(*iFile));

				printf("Changing directory: %s\n", iFile->filename);

				Destination->ChangeDirectory(&(*iFile));
				Source->ChangeDirectory(&(*iFile));

				int	 dReference	 = 0;

				for (dFile = Source->pDirectory->Files.begin(); dFile != Source->pDirectory->Files.end(); dFile++) {
					CopyObject((char *) dFile->filename, Destination, Source);

					dFile = Source->pDirectory->Files.begin(); 

					for (int n=0; n<dReference; n++) { dFile++; }

					dReference++;
				}

				printf("Leaving directory\n");
				Destination->Parent();
				Source->Parent();

				iFile	= Source->pDirectory->Files.begin();

				for (int n=0; n<cReference; n++) { iFile++; }
			} else {
				void	*pFile	= malloc(iFile->Length);

				Source->ReadFile(&(*iFile), pFile);
				Destination->WriteFile(&(*iFile), pFile);

				free(pFile);
			}
		}

		cReference++;
	}

	return 0;
}

int _tmain(int argc, _TCHAR* argv[]) {
	printf("ADFSCopy 1.0 - By Richard Gellman\n");

	if (argc<3) {
		printf("Syntax: %s <image file> <Physical Drive Number>\n", argv[0]);

		return 1;
	}

	DataSource	*pImage	= new IDE8Source(new ImageDataSource(argv[1]));

	char	Drive[64];
	sprintf_s(Drive, 64, "\\\\.\\PhysicalDrive%s", argv[2]);

	DataSource	*pDrive	= new IDE8Source(new ImageDataSource(Drive));

	Source	= new ADFSFileSystem(pImage);
	Destination	= new ADFSFileSystem(pDrive);

	printf("Starting in $\n");

	CopyObject("MENU", Destination, Source);
	CopyObject("GAMES", Destination, Source);

	return 0;
}

