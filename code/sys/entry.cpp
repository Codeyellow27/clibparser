#include "/include/io"
#include "/include/shell"
// WELCOME
int welcome() {
    set_fg(0, 240, 240);
    set_bg(10, 10, 10);
    shell("cat /usr/logo.txt");
    restore_fg();
    restore_bg();
    put_string("\n\n");
    set_fg(160, 160, 160);
    put_string("Welcome to @clibos system by bajdcc!\n\n");
    put_string("# Type \"help\" for help.\n\n");
    restore_fg();
}
int main(int argc, char **argv) {
    welcome();
    exec("sh"); wait();
    return 0;
}