# @TEST-EXEC: spicyz -o test.hlto test.evt test.spicy
# @TEST-EXEC: zeek -r ${TRACES}/ssh-single-conn.trace test.hlto %INPUT >output
# @TEST-EXEC: btest-diff output
#
# @TEST-DOC: In EVT, provide access to hooks arguments

event Banner::error(msg: string) {
    print fmt("Error message: %s", msg);
}

# @TEST-START-FILE test.spicy
module SSH;

public type Banner = unit {
    magic   : /SSH-/;
    version : /[^-]*/;
    dash    : /-/;
    software: /KAPUTT/;
};
# @TEST-END-FILE

# @TEST-START-FILE test.evt

protocol analyzer spicy::SSH over TCP:
    parse originator with SSH::Banner,
    port 22/tcp;

on SSH::Banner::%error(msg: string) -> event Banner::error(msg);
on SSH::Banner::%error() -> event Banner::error("n/a");

# @TEST-END-FILE
