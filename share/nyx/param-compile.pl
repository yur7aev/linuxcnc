#!/usr/bin/perl

# $p[group][axis][param]

my @param;
my @mask;

my %g = (
	# J3/J4
	PA => 0,
	PB => 1,
	PC => 2,
	PD => 3,
	PE => 4,
	PF => 5,
	PG => 6,
	PH => 7,
	PJ => 8,
	PO => 9,
	PS => 10,
	PL => 11,
	PT => 12,
	PN => 13,
	# J2/J2S
	P => 0,		# 0-based
	# MDS
	SV => 0,
	SP => 1,
	# MTL
	PN0 => 0,
	PN1 => 0,
	PN2 => 0,
	PN3 => 0,
	PN4 => 0,
	PN5 => 0,
	PN6 => 0,
	PN7 => 0,
	PN8 => 0,
);

die "usage:\n$0 TEXT-PARAM-FILE\n" unless @ARGV > 0;
my $name = $ARGV[0];
open F, "<$name" or die "can't open input file";

while (<F>) {
	s/;.*//;
	chomp;
	next if /^\s*$/;

	my ($pn, @p) = split /\s+/;

	if ($pn =~ /^([a-z]+)(\d+)/i) {
		my $gn = uc($1);
		my $g = $g{$gn};
		my $n = $2;
		--$n if $gn ne "P";

		die "invalid param group $pn" unless defined $g;
		die "invalid param no $n " if $n < 0;

		my $a = 0;
		for (@p) {
			s/\.//;
			if (/(^0x|^#)[0-9a-fA-F]+$/) {
				$_ = hex;
			} elsif (/^\d+$/) {
				$_ = int;
			} elsif (/^-+$/) {
				$_ = '';
			} else {
				printf STDERR "invalid param value: P$gn%02d/$a = $_\n", $n+1;
			}

			$param[$a] = [] unless exists $param[$a];
			$param[$a][$g] = [] unless exists $param[$a][$g];
			die "duplicate param $a $g $n" if exists $param[$a][$g][$n];
			$param[$a][$g][$n] = $_;

			$mask[$a] = [] unless exists $mask[$a];
			$mask[$a][$g] = [] unless exists $mask[$a][$g];
			$mask[$a][$g][$n>>4] = 0 unless exists $mask[$a][$g][$n>>4];
			$mask[$a][$g][$n>>4] |= 1 << ($n % 16) unless $_ eq '';
			$a++;
		}

#		print "$g/$n = ", join(",", @{$p{$g}[$n]}), "\n";
	}
}
close F;

$name =~ s/(\.[^\.]+)?$/.bin/;

open FO, ">$name" or die "can't open out file";
binmode FO;

foreach $a (0..$#param) {
	my $ng = @{$param[$a]}-1;
	print FO "PAR ";
	print FO pack "v", $ng+1;
	printf STDERR "$a:";
	foreach $g (0..$ng) {
		$n = @{$param[$a][$g]};
		print FO pack "v", $n;
		printf STDERR "$g:$n ";
	}
	print STDERR "\n";
	foreach $g (0..$ng) {
		my $p = $param[$a][$g];
		my $m = $mask[$a][$g];
		print FO map { pack "v", $_ } @{$m};
		print FO map { pack "v", $_ } @{$p};
		#print STDERR join(",", @{$p}), "/", join(",", @{$m}), "\n";
	}
}

close FO;

<<XXX;

J3__

nA
nB
nC
nD
nE
nF
nG
nH
nJ
nO
nS
nL
nT
nN
0
0

maskA1
maskA2
PA1
...
PA19

maskB1
maskB2
maskB3
PB1
...
PB32

...


struct servo_params {
	uint32_t magic;
	uint16_t np[16];

};

XXX
