#!/usr/bin/env perl
#
# Usage:
#   $ release/changelog.pl
#
# This script adds a new release entry to "CHANGELOG.md".

use integer;
use strict;
use File::Basename qw(fileparse);
use Getopt::Long;
use POSIX qw(strftime);

Getopt::Long::Configure(qw{bundling
                           no_auto_abbrev
                           no_ignore_case
                           require_order});

my ($prog, $dir, $suffix) = fileparse($0, '.pl');
my ($help, $changelog) = (0, 'CHANGELOG.md');

sub fatal {
  warn @_;
  exit 1;
}

sub usage {
  warn @_ if @_;
  warn <<EOT;
Usage: ${dir}${prog}${suffix} [<options>] [<changelog-file>]

 where the options are:
    -h, --help    Display this message.

Patches a new release entry into <changelog-file> (default: ${changelog}).
If <changelog-file> is '-', the entry is instead written to stdout.
EOT
  exit 2;
}

my $getops = GetOptions('h|help' => \$help);
usage if !$getops || $help || @ARGV > 1;
$changelog = shift if @ARGV;

my $year_month = strftime('%Y-%m', localtime);
my $commitish = 'upstream/master';

chomp(my $cur_version = qx{git describe --tags --abbrev=0 ${commitish}});
my ($major, $minor, $patch) = $cur_version =~ m{^v(\d+).(\d+).(\d+)$}
  or fatal "${cur_version}: Unexpected version format\n";
my $new_version = sprintf 'v%d.%d.%d', $major, $minor + 1, 0;  # next minor
my $fut_version = sprintf 'v%d.%d.%d', $major, $minor + 2, 0;  # prediction

# Enumerate commits modifying the argument paths since the last release.
sub commits {
  my $entry = '';
  my $banner = "\n### " . shift . "\n\n";
  open CMD, '-|', qw{git log --no-merges --format=%s},  # subject only
                  "${cur_version}..HEAD", ${commitish}, '--', @_
    or fatal "git log: $!";
  while (my $line = <CMD>) {
    # Skip commits with uninteresting tags.
    next if $line =~ m{^(?:chore|ci|test)(?:\(.*?\))?!?:};
    $entry .= $banner . '* ' . $line;
    $banner = '';
  }
  close CMD or exit 1;
  $entry .= $banner . "* No changes from ${cur_version}\n" if $banner;
  return $entry;
}

# Accumulate the new entry, split by major components.
my $text = "## ${fut_version} - TBD\n";  # new placeholder
$text .= "\n## ${new_version} - ${year_month}\n";
$text .= commits 'Bigtable', 'google/cloud/bigtable';
$text .= commits 'Storage', 'google/cloud/storage';
$text .= commits 'Spanner', 'google/cloud/spanner';
$text .= commits 'Common libraries', 'google/cloud',
                  map(":(exclude)google/cloud/$_",
                      qw{bigtable storage spanner},  # handled above
                      qw{bigquery firestore pubsub});  # unreleased

if ($changelog eq '-') {
  # Just write the new entry to stdout.
  print $text;
} else {
  # Read the old CHANGELOG.md.
  open CHANGELOG, '<', $changelog or fatal "${changelog}: $!\n";
  my @changelog = <CHANGELOG>;
  close CHANGELOG;

  # Replace the old placeholder with the new entry.
  my $oldtext = "## ${new_version} - TBD";
  scalar(grep { s{^${oldtext}\n$}{${text}} } @changelog) == 1
    or fatal "${changelog}: Could not find '${oldtext}'\n";

  # Write the new CHANGELOG.md.
  open CHANGELOG, '>', $changelog and
  print CHANGELOG @changelog and
  close CHANGELOG or fatal "${changelog}: $!";
}

exit 0;
