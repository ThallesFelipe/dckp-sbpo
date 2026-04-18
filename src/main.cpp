#include <filesystem>
#include <iostream>

#include "utils/instance_reader.h"

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        std::cout << "Uso: dckp_sbpo <caminho_da_instancia>\n";
        return 0;
    }

    const std::filesystem::path instance_path{argv[1]};

    DCKPInstance instance;
    if (!instance.read_from_file(instance_path))
    {
        std::cerr << "Erro ao ler instancia: " << instance.last_error() << '\n';
        return 1;
    }

    instance.print();
    return 0;
}
