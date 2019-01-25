
extern "C" int __attribute__((ms_abi)) main(int argc, char** argv) {
    
    if(argc > 0) {
        typedef void (__attribute__((ms_abi)) *PRINTF)(const char* format, ...);

        PRINTF printf = (PRINTF)argv[0];

        printf("Hello World %i\n", 42);
    }

    return 42;

}