class BluetoothSocket {
public:
  int mPort;
  int mType;
  int mFd;
  bool mAuth;
  bool mEncrypt;

  BluetoothSocket();
  void Connect(int channel, const char* bd_address);

protected:
  void InitSocketNative();
};
