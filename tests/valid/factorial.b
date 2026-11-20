/* Recursive factorial - from the phase-1 handout */
factorial: function integer ( n: integer ) = {
    if (n <= 1) return 1;
    return n * factorial(n - 1);
}

main: function integer () = {
    print "factorial(5) = ", factorial(5), "\n";
    return 0;
}
