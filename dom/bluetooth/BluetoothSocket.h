class BluetoothSocket {
public:
  int mPort;
  int mType;
  int mFd;
  bool mAuth;
  bool mEncrypt;

  BluetoothSocket();
  void Connect(int channel, const char* bd_address);
  void Listen(int channel);
  int Accept();
  bool Available();

protected:
  void InitSocketNative(int type, bool auth, bool encrypt);
};
