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

load.latency <- function(filename, run) {
    df <- read.csv(filename, comment.char='#', header=FALSE,
    col.names=c('op', 'bytes', 'ms'))
    df$op <- factor(df$op)
    df$run <- factor(run)
    df$seconds <- df$ms / 1000.0
    return(df);
}

df <- load.latency(arg.filename, arg.run)
aggregate(ms ~ op, data=df, FUN=summary)

aggregate(ms ~ op, data=df, FUN=function(x) quantile(x, prob=c(0.95, 0.99)))

ggplot(data=df, mapping=aes(x=ms, color=op)) +
  geom_density() + xlim(0, 600)

ggsave(paste(arg.run, '.png', sep=''))

q(save="no")
