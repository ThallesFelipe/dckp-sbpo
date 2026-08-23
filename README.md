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

---

## 8. Calibração do ILS + VND com irace

O cenário em [scripts/irace/](scripts/irace/) calibra a força de perturbação
do ILS e o subconjunto de vizinhanças do VND. Cada chamada usa exatamente
**1.000 segundos**, a mesma condição de parada do experimento do artigo. O
irace recebe o lucro com sinal negativo porque ele minimiza custos.

Uma descrição completa, didática e pronta para apresentação está em
[RELATORIO_IRACE.md](RELATORIO_IRACE.md).

### Instalação e verificação

No WSL:

~~~bash
sudo apt install -y r-base
R -q -e 'install.packages("irace", repos="https://cloud.r-project.org")'
make build
chmod +x scripts/irace/*.sh tests/scripts/*.sh
./scripts/irace/run.sh --check-fast
~~~

O check rápido usa 100 ms exclusivamente para validar o encadeamento entre
irace, target runner e executável. O wrapper localiza automaticamente o
executável instalado dentro da biblioteca pessoal do R.

### Protocolo de tuning

- Treino: variantes I1–I4 das 20 famílias, totalizando 80 instâncias.
- Blocos: quatro blocos balanceados, cada um contendo uma instância de cada
  família; uma configuração só pode ser eliminada após um bloco completo.
- Teste independente: variante I5 das 20 famílias.
- Orçamento: 1.000 avaliações do target runner.
- Tempo: 1.000 segundos por avaliação.
- Execução sequencial para evitar interferência entre processos medidos por
  tempo.
- Seed do irace: 20260810.
- Espaço: perturbação inteira de 1 a 8 e 15 subconjuntos não vazios das quatro
  vizinhanças do VND.
- A configuração padrão (perturbação 4 e todas as vizinhanças) participa como
  configuração inicial.

Execute a calibração definitiva com:

~~~bash
./scripts/irace/run.sh
~~~

No pior caso, 1.000 avaliações de 1.000 segundos correspondem a cerca de
11,6 dias de CPU. Cada execução cria em results/irace/ um log RData com nome
único e um manifesto contendo commit Git, estado do worktree, versões,
identificadores criptográficos do binário/cenário, sistema e processador.

Extraia a melhor configuração e o comando exato da avaliação final com:

~~~bash
Rscript scripts/irace/extract_best.R results/irace/tuning_<timestamp>.Rdata
~~~

### Avaliação final

Depois de obter os cinco valores da configuração vencedora, execute:

~~~bash
./scripts/irace/evaluate_test.sh PERTURBACAO ADD SWAP_1_1 SWAP_2_1 SWAP_1_2
~~~

O script compara a configuração padrão e a calibrada nos mesmos pares de
instância/seed: 20 instâncias de teste × 5 runs × 2 configurações, totalizando
200 execuções de 1.000 segundos. A ordem entre baseline e configuração
calibrada é alternada para reduzir viés temporal.

Além do CSV bruto, são gerados automaticamente:

- médias pareadas por instância;
- ganho absoluto e percentual da configuração calibrada;
- vitórias, empates e derrotas;
- teste de Wilcoxon sobre os 20 ganhos percentuais por instância;
- resumo de lucro e tempo.

Essa bateria pode consumir cerca de 55,6 horas em execução sequencial.
