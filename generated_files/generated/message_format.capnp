# Generated ID
@0xa7b5195f17d5287f;

# Set a namespace
using Cxx = import "/capnp/c++.capnp";
$Cxx.namespace("cpnpro");

struct BroadcastData {
  union {
    # Cap'n Proto FORCES camelCase to be used for these...
    d     @0  : Float64;
    rD    @1  : List(Float64);
    f     @2  : Float32;
    rF    @3  : List(Float32);
    i8    @4  : Int8;
    rI8   @5  : List(Int8);
    i16   @6  : Int16;
    rI16  @7  : List(Int16);
    i32   @8  : Int32;
    rI32  @9  : List(Int32);
    i64   @10 : Int64;
    rI64  @11 : List(Int64);
    u8    @12 : UInt8;
    rU8   @13 : List(UInt8);
    u16   @14 : UInt16;
    rU16  @15 : List(UInt16);
    u32   @16 : UInt32;
    rU32  @17 : List(UInt32);
    u64   @18 : UInt64;
    rU64  @19 : List(UInt64);
    str   @20 : Text;
    rStr  @21 : List(Text);
    # TODO: Actually figure out how to use bytes?
    # bytes @22 : Data;
  }
}

struct Broadcast {
  messageID @0 : UInt32;
  tagID @1 : Text;
  origin @2 : Text;
  hopsLeftP1 @3 : UInt8;
  data @4 : BroadcastData;
}

struct Greeting {
  from @0 : Text;
  neighbors @1 : List(Text);
}

# struct Goodbye {
#   # Empty
# }

struct NewNeighbor {
  neighborID @0 : Text;
}

struct RemoveNeighbor {
  neighborID @0 : Text;
}

struct Message {
  union {
    broadcast @0 : Broadcast;
    greeting @1 : Greeting;
    goodbye @2 : Void;
    newNeighbor @3 : NewNeighbor;
    removeNeighbor @4 : RemoveNeighbor;
    heartbeat @5 : Void;
  }
}
