pragma solidity ^0.4.23;

contract Update {  
  address owner;
  string host;
  uint16 port;
  string path;
  string device_type;
  uint256 tid;
  uint256 success;
  uint256 failure;
  uint256 timestamp;
  mapping (uint256 => string) tid2md5;
  
  constructor(string new_type) public {
      owner = msg.sender;
      tid = 0;
      device_type = new_type;
  }
  
  modifier onlyOwner() {
      require(msg.sender == owner);
      _;
  }

  event UPDATE(string host, uint16 port, string path, uint256 tid, string device_type);
  
  function get_host() public view returns (string) {
      return host;
  }
  
  function get_port() public view returns (uint16) {
      return port;
  }
  
  function get_path() public view returns (string) {
      return path;
  }
  
  function get_tid() public view returns (uint256) {
      return tid;
  }
  
  function get_md5(uint256 new_tid) public view returns (string) {
      return tid2md5[new_tid];
  }
  
  function get_type() public view returns (string) {
      return device_type;
  }
  
  function string_eq(string a, string b) private pure returns (bool) {
      return bytes(a).length == bytes(b).length && keccak256(a) == keccak256(b);
  }
  
  function update_fw (uint256 new_tid, string new_type, string new_host, uint16 new_port, string new_path, string new_md5) public onlyOwner {
      require(new_tid > tid);
      require(string_eq(new_type, device_type));
      host = new_host;
      port = new_port;
      path = new_path;
      success = 0;
      failure = 0;
      tid = new_tid;
      tid2md5[tid] = new_md5;
      notify();
      timestamp = now;
  }

  function notify() private onlyOwner {
    emit UPDATE(host, port, path, tid, device_type);
  }
  
  function get_success() public view returns (uint256) {
      return success;
  }
  
  function get_failure() public view returns (uint256) {
      return failure;
  }
  
  function get_timestamp() public view returns (uint256) {
      return timestamp;
  }
  
  function verify(uint256 new_tid, string md5) public returns (bool) {
    if(new_tid == tid && string_eq(md5, tid2md5[new_tid])){
        success = success + 1;
        return true;
    } else {
        failure = failure + 1;
        return false;
    }
  }
}
