args <- commandArgs(trailingOnly = TRUE)
if (length(args) != 1L) {
    stop("Usage: Rscript analyze_test.R RESULTS.csv")
}

input <- normalizePath(args[[1L]], mustWork = TRUE)
data <- read.csv(input, stringsAsFactors = FALSE)

required <- c("configuration", "instance", "run", "seed", "profit", "time_ms", "valid")
if (!all(required %in% names(data))) {
    stop("Results CSV is missing required columns")
}
if (nrow(data) != 200L || any(!data$valid)) {
    stop("Expected exactly 200 valid results")
}
if (!setequal(unique(data$configuration), c("baseline", "tuned"))) {
    stop("Expected baseline and tuned configurations")
}

per_instance <- aggregate(
    cbind(profit, time_ms) ~ configuration + instance,
    data = data,
    FUN = mean
)
names(per_instance)[names(per_instance) == "profit"] <- "mean_profit"
names(per_instance)[names(per_instance) == "time_ms"] <- "mean_time_ms"

baseline <- per_instance[per_instance$configuration == "baseline", c("instance", "mean_profit", "mean_time_ms")]
tuned <- per_instance[per_instance$configuration == "tuned", c("instance", "mean_profit", "mean_time_ms")]
names(baseline)[2:3] <- c("baseline_mean_profit", "baseline_mean_time_ms")
names(tuned)[2:3] <- c("tuned_mean_profit", "tuned_mean_time_ms")
paired <- merge(baseline, tuned, by = "instance", all = FALSE, sort = TRUE)

if (nrow(paired) != 20L) {
    stop("Expected 20 paired instance summaries")
}

paired$absolute_gain <- paired$tuned_mean_profit - paired$baseline_mean_profit
paired$relative_gain_pct <- 100 * paired$absolute_gain / paired$baseline_mean_profit

nonzero <- paired$relative_gain_pct[paired$relative_gain_pct != 0]
p_value <- if (length(nonzero) == 0L) {
    1
} else {
    wilcox.test(paired$relative_gain_pct, mu = 0, exact = FALSE)$p.value
}

summary <- data.frame(
    metric = c(
        "instances", "runs_per_instance_per_configuration", "total_runs_per_configuration", "baseline_mean_profit",
        "tuned_mean_profit", "mean_relative_gain_pct", "median_relative_gain_pct",
        "wins", "ties", "losses", "wilcoxon_p_value",
        "baseline_mean_time_ms", "tuned_mean_time_ms"
    ),
    value = c(
        nrow(paired), 5, nrow(data) / 2,
        mean(data$profit[data$configuration == "baseline"]),
        mean(data$profit[data$configuration == "tuned"]),
        mean(paired$relative_gain_pct), median(paired$relative_gain_pct),
        sum(paired$absolute_gain > 0), sum(paired$absolute_gain == 0),
        sum(paired$absolute_gain < 0), p_value,
        mean(data$time_ms[data$configuration == "baseline"]),
        mean(data$time_ms[data$configuration == "tuned"])
    )
)

base <- sub("\\.csv$", "", input, ignore.case = TRUE)
paired_path <- paste0(base, "_per_instance.csv")
summary_path <- paste0(base, "_summary.csv")
write.csv(paired, paired_path, row.names = FALSE)
write.csv(summary, summary_path, row.names = FALSE)

print(summary, row.names = FALSE)
cat("Per-instance analysis:", paired_path, "\n")
cat("Summary:", summary_path, "\n")
