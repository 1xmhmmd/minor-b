main: function integer () = {
    a: integer = 1 + 2 * 3 - 4 / 2 % 3;
    b: integer = -a ^ 2;
    c: integer = 2 ^ 3 ^ 2;
    d: boolean = !(a < b) && (b >= c || c != 0) && a == b;
    e: boolean = a <= b;
    arr: array [3] integer = {1, 2, 3};
    i: integer = 0;
    i++;
    i--;
    arr[i] = arr[i + 1] = 7;
    a = b = c;
    x: integer = f(g(1), h(), arr[0]);
    return -(a + b) * 2;
}
