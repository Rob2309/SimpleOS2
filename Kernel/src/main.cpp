
int Test() {
    return 42;
}

extern "C" int main(int argc, char** argv) {
    return Test();
}