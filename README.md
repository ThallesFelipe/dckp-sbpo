# DCKP-SBPO

---

## 1. Pré-requisitos

A máquina precisa ter os seguintes pacotes instalados:

- `g++` (com suporte a C++23 — versão **13 ou superior**)
- `cmake` (versão **3.24 ou superior**)
- `ninja-build`
- `make`
- `bash`, `awk`, `find`

### Instalação no Ubuntu 24.04 (ou WSL Ubuntu 24.04)

```bash
sudo apt update
sudo apt install -y build-essential g++ cmake ninja-build make
```

Verifique as versões:

```bash
g++ --version       # esperado: g++ >= 13
cmake --version     # esperado: cmake >= 3.24
ninja --version
```

---

## 2. Compilação

Na raiz do projeto, execute:

```bash
make build
```

Isso vai:

1. Configurar o CMake em modo **Release** dentro de [build/](build/).
2. Compilar com Ninja em paralelo.
3. Gerar o binário em [build/dckp_sbpo](build/dckp_sbpo).

Para limpar e recompilar do zero:

```bash
make clean
make build
```

---

## 4. Executar todas as instâncias (gera o CSV)

O script [scripts/run_experiments.sh](scripts/run_experiments.sh) roda **todas as 100 instâncias** (I1–I20, 5 instâncias por grupo), executa o algoritmo escolhido **N vezes por instância** com sementes diferentes, e escreve um CSV.

Dê permissão de execução (apenas na primeira vez):

```bash
chmod +x scripts/run_experiments.sh
```

Execute o experimento principal:

```bash
./scripts/run_experiments.sh --algo ILS --time-limit 600000 --runs 5
```

Ou só:

```bash
./scripts/run_experiments.sh
```

para usar os padrões (ILS, 10 min por execução, 5 runs por instância).

Parâmetros:

| Flag             | Significado                                   | Padrão |
|------------------|-----------------------------------------------|--------|
| `--algo`         | `Greedy_MaxProfit` \| `VND` \| `ILS` \| `VNS` | `ILS`  |
| `--time-limit`   | Tempo máximo por execução, em **ms**          | `600000` (10 min) |
| `--runs`         | Nº de execuções por instância                 | `5`    |
| `--output`       | Caminho do CSV de saída                       | `results/results_<algo>_<timestamp>.csv` |

---

## 5. Onde fica o resultado

Ao final, o CSV é gravado em:

```
results/results_ILS_<YYYYMMDD_HHMMSS>.csv
```

O caminho absoluto também é impresso na última linha do log do script.

### Formato do CSV

Cabeçalho:

```
instance,run,seed,profit,weight,capacity,start_time,end_time,time_ms,valid,algorithm
```

Ao final do arquivo são adicionadas linhas de comentário (`#`) com estatísticas agregadas por grupo (Group 1: 1I1–10I5, Group 2: 11I1–20I5) e o total geral: melhor lucro, média, mediana, desvio-padrão e tempo médio.

---

## 6. Execução de uma única instância (opcional)

Para rodar apenas uma instância manualmente:

```bash
./build/dckp_sbpo DCKP-instances/DCKP-instances-set-I-100/I1-I10/1I1 \
    --algo ILS --time-limit 600000 --seed 42
```

Saída humana (sem `--csv`):

```
instance=1I1 algorithm=ILS profit=... weight=... capacity=... time_ms=... valid=true
```

Adicione `--csv` para emitir uma única linha CSV (sem cabeçalho), ou `--verbose` para imprimir detalhes da instância e da solução.

---

## 7. Resumo — passo a passo do zero

```bash
# 1. Instalar dependências
sudo apt update
sudo apt install -y build-essential g++ cmake ninja-build make

# 2. Entrar no projeto
cd dckp-sbpo

# 3. Compilar
make build

# 4. Rodar todos os experimentos (gera o CSV em results/)
chmod +x scripts/run_experiments.sh
./scripts/run_experiments.sh --algo ILS --time-limit 600000 --runs 5
```

O CSV final estará em [results/](results/).
