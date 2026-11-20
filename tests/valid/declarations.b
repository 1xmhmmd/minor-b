// every kind of declaration from the language reference
x: integer;
y: integer = 123;
b: boolean = false;
c: char = 'q';
s: string = "hello world\n";
a: array [5] integer;
a2: array [5] integer = {1, 2, 3, 4, 5};
flags: array [2] boolean = {true, false};

writechar: function void ( c: char );
puts: function void ( s: string );
square: function integer ( x: integer ) = {
    return x^2;
}
printarray: function void ( a: array [] integer, size: integer ) = {
    i: integer;
    for (i = 0; i < size; i++) {
        print a[i], "\n";
    }
}
main: function integer () = {
    puts("hello world");
    return 0;
}
