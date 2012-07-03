#include "FakeFileSystem.h"

FakeFileSystem::FakeFileSystem()
{
  char name[] = "sdcard";
  root = new FileEntity(1, "sdcard", sizeof(name), 500);

  for (int i = 0;i < 10;++i) {
    char fileName[] = "Folder_00";

    fileName[8] = '0' + i;

    root->files[i] = new FileEntity(1, fileName, sizeof(fileName), 6543);
  }
    
  for (int i = 10;i < FileEntity::MAX_FILE_COUNT;++i) {
    char fileName[] = "TEST00.txt";

    fileName[4] = '0' + (i / 10);
    fileName[5] = '0' + (i % 10);

    root->files[i] = new FileEntity(0, fileName, sizeof(fileName), 550);
  }
}

FileEntity* FakeFileSystem::GetRoot()
{
  return root;
}
