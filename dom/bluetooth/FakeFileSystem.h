class FileEntity
{
public:
  const static int MAX_FILE_COUNT = 30;

  FileEntity(int aType, char* aName, int aNameLength, int aSize) : type(aType), size(aSize) {
    for (int i = 0;i < aNameLength;++i) {
      name[i] = aName[i];
    }
  };  
  
  FileEntity* files[MAX_FILE_COUNT];
  int GetType() { return type; }

private:  
  int type;  // 0: file, 1: folder  
  char name[255];
  int size;
};

class FakeFileSystem
{
public:
  FakeFileSystem();

  FileEntity* GetRoot();

private:
  FileEntity* root;
};
