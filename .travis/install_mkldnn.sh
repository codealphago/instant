if [ ! -d "$HOME/mkl-dnn/build/src" ]; then
    git clone https://github.com/01org/mkl-dnn.git
    cd mkl-dnn/scripts && bash ./prepare_mkl.sh && cd ..
    sed -i 's/add_subdirectory(examples)//g' CMakeLists.txt
    sed -i 's/add_subdirectory(tests)//g' CMakeLists.txt
    mkdir -p build && cd build && cmake .. && make
    sudo make install
    cd ..
else
    echo "Using cached directory."
fi