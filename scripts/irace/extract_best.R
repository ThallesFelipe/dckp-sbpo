args <- commandArgs(trailingOnly = TRUE)
if (length(args) != 1L) {
    stop("Usage: Rscript extract_best.R TUNING.Rdata")
}

if (!requireNamespace("irace", quietly = TRUE)) {
    stop("The irace R package is not installed")
}

log_file <- normalizePath(args[[1L]], mustWork = TRUE)
results <- irace::read_logfile(log_file)
elite <- irace::getFinalElites(results, n = 1L, drop.metadata = TRUE)

required <- c(
    "perturbation_strength", "vnd_add", "vnd_swap_1_1",
    "vnd_swap_2_1", "vnd_swap_1_2"
)
if (nrow(elite) != 1L || !all(required %in% names(elite))) {
    stop("The log does not contain one compatible final elite configuration")
}

values <- unlist(elite[1L, required], use.names = FALSE)
cat("Best configuration:\n")
print(elite[1L, required], row.names = FALSE)
cat("\nFinal evaluation command:\n")
cat("./scripts/irace/evaluate_test.sh", paste(values, collapse = " "), "\n")
