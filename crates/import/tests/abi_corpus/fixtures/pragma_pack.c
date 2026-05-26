#pragma pack(push, 1)
struct Packet {
    char tag;
    int value;
};
#pragma pack(pop)

int packet_size(void) { return sizeof(struct Packet); }
int packet_align(void) { return _Alignof(struct Packet); }
