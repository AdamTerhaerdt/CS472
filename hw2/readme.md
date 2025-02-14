### Sample Output from my implementation
## Author: Adam Terhaerdt
## Date: 02-14-2025

```
./client -p "Hello Adam"
HEADER VALUES 
  Proto Type:    PROTO_CS_FUN
  Proto Ver:     VERSION_1
  Command:       CMD_PING_PONG
  Direction:     DIR_RECV
  Term:          TERM_FALL 
  Course:        NONE
  Pkt Len:       23

RECV FROM SERVER -> PONG: Hello Adam

./client -c cs500
HEADER VALUES 
  Proto Type:    PROTO_CS_FUN
  Proto Ver:     VERSION_1
  Command:       CMD_CLASS_INFO
  Direction:     DIR_RECV
  Term:          TERM_FALL 
  Course:        cs500
  Pkt Len:       12

RECV FROM SERVER -> Requested Course Not Found

./client -c cs577
HEADER VALUES 
  Proto Type:    PROTO_CS_FUN
  Proto Ver:     VERSION_1
  Command:       CMD_CLASS_INFO
  Direction:     DIR_RECV
  Term:          TERM_FALL 
  Course:        cs577
  Pkt Len:       12

RECV FROM SERVER -> CS577: Software architecture is importan
```