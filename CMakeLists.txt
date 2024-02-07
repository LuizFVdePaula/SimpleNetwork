# Especifica a versão mínima do CMake necessária
cmake_minimum_required(VERSION 3.12)

# Define o nome do projeto
project(MyNetwork)

# Define as versões mínimas das linguagens
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Especifique a localização dos cabeçalhos da Asio
set(ASIO_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/include)

# Incluir diretórios
include_directories(${ASIO_INCLUDE_DIR})

# Adiciona arquivos de origem ao projeto
add_executable(client src/simple_client.cpp)
add_executable(server src/simple_server.cpp)