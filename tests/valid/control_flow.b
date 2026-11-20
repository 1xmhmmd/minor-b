main: function integer () = {
    i: integer;
    for (i = 0; i < 10; i++) {
        if (i % 2 == 0)
            print "even ", i, "\n";
        else if (i == 5)
            print "five\n";
        else
            print "odd\n";
    }
    for (;;) { return 1; }
    if (true) if (false) print "a"; else print "dangling else binds to inner if";
    return;
}
