# DCKP-SBPO

---

## 1. Pré-requisitos

- `g++`
- `clang++-18`, `clangd-18`, `clang-tidy-18` e `clang-format-18`
- `cmake`
- `ninja-build`
- `make`
- `bash`, `awk`, `find`
- `cppcheck`, `shellcheck`, `valgrind` e `gcovr`

No PowerShell, atualize o WSL:

```powershell
wsl --update
wsl --shutdown
```

Atualize o sistema e instale a toolchain:

```bash
sudo apt update
sudo apt full-upgrade -y
sudo apt install -y \
    build-essential gcc g++ clang clang-tidy clang-format clang-tools clangd-18 \
    cmake ninja-build make cppcheck iwyu shellcheck valgrind gcovr lcov \
    gdb ccache hyperfine time python3
```

Verifique as versões:

```bash
g++ --version
cmake --version
ninja --version
clangd-18 --version
```

---

## 2. Compilação

Na raiz do projeto, execute:

```bash
make build
```

Para limpar e recompilar do zero:

```bash
make clean
make build
```

---

## 3. Auditoria completa

Execute toda a matriz reproduzível com:

```bash
make audit
```

A auditoria falha imediatamente se encontrar um diagnóstico real. Ela inclui:

- GCC 13 e Clang 18 em Release, hardening, conjunto estrito de warnings e `-Werror`;
- Clang com AddressSanitizer e UndefinedBehaviorSanitizer;
- analisadores estáticos do GCC, clang-tidy e cppcheck;
- clangd 18 com includes ausentes e não utilizados em modo estrito;
- ShellCheck, validação de Bash e JSON;
- Valgrind nos nove binários de teste e na CLI;
- cobertura mínima protegida contra regressão: 80% de linhas e 45% de branches;
- 19 testes CTest, smoke test das 100 instâncias e validação dos logs do ILS.

O relatório detalhado de cobertura fica em [cmake-build-audit-coverage/coverage.txt](cmake-build-audit-coverage/coverage.txt). O `-fanalyzer` do GCC 13 desativa somente o diagnóstico espúrio de valor não inicializado produzido dentro da própria libstdc++; a mesma categoria permanece coberta pelo analisador do Clang, ASan/UBSan e Valgrind.

---

## 4. Executar todas as instâncias (gera o CSV)

O script [scripts/run_experiments.sh](scripts/run_experiments.sh) roda **todas as 100 instâncias** (I1–I20, 5 instâncias por grupo), executa o algoritmo escolhido **N vezes por instância** com sementes diferentes, e escreve um CSV.

Dê permissão de execução (apenas na primeira vez):

```bash
chmod +x scripts/run_experiments.sh
```

Execute o experimento principal:

```bash
./scripts/run_experiments.sh --algo ILS --time-limit 1000000 --runs 5
```

Ou só:

```bash
./scripts/run_experiments.sh
```

para usar os padrões (ILS, 16 min 40 s por execução, 5 runs por instância).

Parâmetros:

| Flag           | Significado                                       | Padrão                                   |
| -------------- | ------------------------------------------------- | ---------------------------------------- |
| `--algo`       | `Greedy_MaxProfit` \| `VND` \| `ILS` \| `VNS`     | `ILS`                                    |
| `--time-limit` | Tempo máximo por execução, em **ms**              | `1000000` (16 min 40 s)                  |
| `--runs`       | Nº de execuções por instância                     | `5`                                      |
| `--base-seed`  | Primeira semente determinística                   | `42`                                     |
| `--output`     | Caminho do CSV de saída                           | `results/results_<algo>_<timestamp>.csv` |
| `--verbose`    | Logs detalhados de cada iteração do ILS em stderr | desativado                               |

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
    --algo ILS --time-limit 1000000 --seed 42
```

Saída humana (sem `--csv`):

```
instance=1I1 algorithm=ILS profit=... weight=... capacity=... time_ms=... valid=true
```

Adicione `--csv` para emitir uma única linha CSV (sem cabeçalho), ou `--verbose` para imprimir detalhes da instância e da solução. Com ILS, `--verbose` registra cada rodada com lucro candidato, lucro anterior e posterior, aceitação, ganho absoluto e percentual, melhor lucro, trabalho acumulado e tempo decorrido. O resumo final registra as iterações, melhorias e ganho total. Esses logs vão para stderr, portanto o CSV em stdout permanece válido.

---

## 7. Resumo — passo a passo do zero

```bash
# 1. Atualizar e instalar dependências no Ubuntu WSL
sudo apt update
sudo apt full-upgrade -y
sudo apt install -y build-essential g++ clang clang-tidy clang-tools clangd-18 \
    cmake ninja-build make cppcheck shellcheck valgrind gcovr

# 2. Entrar no projeto
cd dckp-sbpo

# 3. Compilar
make build

# 4. Auditar
make audit

# 5. Rodar todos os experimentos (gera o CSV em results/)
chmod +x scripts/run_experiments.sh
./scripts/run_experiments.sh --algo ILS --time-limit 1000000 --runs 5
```

O CSV final estará em [results/](results/).
