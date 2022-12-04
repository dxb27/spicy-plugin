# @TEST-DOC: Tests that parsers can resynchronize on gaps.
#
# @TEST-REQUIRES: spicy-version 10400
# @TEST-EXEC: spicyz -o analyzer.hlto analyzer.spicy analyzer.evt
# @TEST-EXEC: ${ZEEK} -Cr ${TRACES}/gap-recovery.pcap analyzer.hlto Spicy::enable_print=T >output 2>&1
# @TEST-EXEC: if ${SCRIPTS}/spicy-version 10503; then btest-diff output; else OUT=output-before-spicy-issue-1303; mv output "$OUT"; btest-diff "$OUT"; fi

# @TEST-START-FILE analyzer.evt
protocol analyzer spicy::HTTP over TCP:
    parse originator with test::Requests,
    parse responder with test::Responses,
    port 9000/tcp,
    replaces HTTP;
# @TEST-END-FILE

# @TEST-START-FILE analyzer.spicy
module test;
public type Requests = unit {
    %port = 9000/tcp &originator;
    requests: (Request &synchronize)[] foreach { confirm; }
    on %done { print self; }
};

type Request = unit {
    marker: /GET/;
    : /[^\r?\n]+\r?\n/;
    : (/[^\r?\n]*:[^\r?\n]*\r?\n/)[];
    : /\r?\n/;
};

public type Responses = unit {
    %port = 9000/tcp &responder;
    responses: (Response &synchronize)[] foreach { confirm; }
    on %done { print self; }
};

type Response = unit {
    marker: /HTTP/;
    : /[^\r?\n]+\r?\n/;
    : /content-length: /;
    len: bytes &until=b"\r\n" &convert=cast<uint64>($$.to_int());
    : (/[^\r?\n]*:[^\r?\n]*\r?\n/)[];
    : /\r?\n/;
    : bytes &size=self.len;
};
# @TEST-END-FILE
