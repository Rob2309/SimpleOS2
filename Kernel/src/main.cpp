
int g_ReturnValue = 40;

int Test() {
    g_ReturnValue += 2;
    return g_ReturnValue;
}

extern "C" int main(int argc, char** argv) {
    return Test();
}