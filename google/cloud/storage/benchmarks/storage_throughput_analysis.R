#!/usr/bin/env Rscript
# Copyright 2018 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

require(ggplot2)

args <- commandArgs(trailingOnly=TRUE)
if (length(args) != 2) {
    file.arg.name <- "--file="
    all.args <- commandArgs(trailingOnly=FALSE)
    cmd <- basename(sub(file.arg.name, "", all.args[grep(file.arg.name, all.args)]))
    stop(paste("Usage:", cmd, "<benchmark-output-file> <label>"))
}

arg.filename <- args[1]
arg.run <- args[2]

load.throughput <- function(filename, run) {
    df <- read.csv(filename, comment.char='#', header=FALSE,
    col.names=c('op', 'bytes', 'ms'))
    df$op <- factor(df$op)
    df$run <- factor(run)
    df$seconds <- df$ms / 1000.0
    df$bits <- 8 * df$bytes
    df$MiB <- df$bytes / (1024 * 1024.0)
    df$Mbits <- df$bits / 10000000.0
    df$Mbps <- df$Mbits / df$seconds
    return(df);
}

df <- load.throughput(arg.filename, arg.run)
MiB.max <- max(df$MiB)
aggregate(Mbps ~ MiB + op, data=subset(df, MiB == MiB.max), FUN=summary)

# Skip the DELETE operations in this graph because they have "0" throughput.
ggplot(data=df, mapping=aes(x=MiB, y=Mbps, color=run)) +
    scale_color_brewer(palette="Set1") + facet_grid(op ~ .) + ylim(0, 250) +
    geom_quantile(formula=y~log(x), quantiles=c(0.5)) +
    geom_point(size=0.15, alpha=0.2)

fig.height <- function(width) {
    phi <- (1 + sqrt(5.0)) / 2
    return(width / phi)
}

ggsave(paste(arg.run, '.png', sep=''), width=8.5, height=fig.height(8.5))

q(save="no")
