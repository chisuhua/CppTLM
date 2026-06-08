from enum import IntEnum, Enum


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


class ModuleType(str, Enum):
    ROUTER_TLM = "RouterTLM"
    NIC_TLM = "NICTLM"
    CACHE_TLM = "CacheTLM"
    CROSSBAR_TLM = "CrossbarTLM"
    MEMORY_TLM = "MemoryTLM"
    # CPUSim/CpuCluster are DEPRECATED in CppTLM v2.1
    # Use CPU_TLM = "CPUTLM" for new code (registered by default)
    # CPU_SIM is only available when BUILD_LEGACY_MODULES=ON
    CPU_SIM = "CPUSim"
    BUS_SIM = "BusSim"
    TRAFFIC_GEN_TLM = "TrafficGenTLM"
    DIRECTORY_CTRL = "DirectoryCtrl"
