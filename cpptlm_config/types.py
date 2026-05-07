from enum import IntEnum


class RouterPort(IntEnum):
    NORTH = 0
    EAST = 1
    SOUTH = 2
    WEST = 3
    LOCAL = 4


class NICPort(IntEnum):
    PE_REQ = 0
    PE_RESP = 1
    NET_REQ = 2
    NET_RESP = 3
