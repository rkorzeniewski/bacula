#!/usr/bin/perl -w

=head1 LICENSE

   Bweb - A Bacula web interface
   BaculaÂ® - The Network Backup Solution

   Copyright (C) 2000-2010 Free Software Foundation Europe e.V.

   The main author of Bweb is Eric Bollengier.
   The main author of Bacula is Kern Sibbald, with contributions from
   many others, a complete list can be found in the file AUTHORS.
   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation and included
   in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   Affero General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.

   Bacula® is a registered trademark of Kern Sibbald.
   The licensor of Bacula is the Free Software Foundation Europe
   (FSFE), Fiduciary Program, Sumatrastrasse 25, 8006 Zürich,
   Switzerland, email:ftf@fsfeurope.org.

=cut

use strict ;
use GD qw/gdSmallFont/;

package GTime ;

my $pi =  3.14159265;

our $gd ;
our @color_tab ;

our $black ;
our $white ;

our $last_level = 1 ;
our @draw_label ;
our $height ;
our $width ;

our $cur_color = 1 ;

my $debug = 1 ;
my $font_size = 6 ;

our @image_map ;

sub new
{
    my ($class, %arg) = @_ ;

    my $self = {
        height     => 800,      # element height in px
        width      => 600,      # element width in px
        xmarge     => 150,

        cbegin     => 0,        # compute begin
        cend       => 0,        # compute end

        begin      => 0,        # user start
        end        => 0,        # user end

        cur_y      => 0,        # current y position
        elt_h      => 20,       # elt height in px

        data       => [],
        type       => {},

        base_url   => '',
    } ;

    map { $self->{$_} = $arg{$_} } keys %arg ;

    if ($self->{begin}) {
        $self->{begin} = parsedate($self->{begin});
    }
    if ($self->{end}) {
        $self->{end} = parsedate($self->{end});
    }

    bless $self ;

    return $self ;
}

my $_image_map = '';

sub get_imagemap
{
    my ($self, $title, $img) = @_ ;

    return "
<map name='testmap'>
    $_image_map
</map>
<img src='$img' border=0 usemap='#testmap' alt=''>
" ;

}

sub push_image_map
{
    my ($self, $label, $tips, $x1, $y1, $x2, $y2) = @_ ;

    my $ret = sprintf("%.2f,%.2f,%.2f,%.2f,%.2f,%.2f",
                      $x1,$y1,$x1,$y2,$x2,$y2);

    $_image_map .= "<area shape='polygon' coords='$ret' ".
                   "title='$tips' href='$self->{base_url}/$label'>\n" ;
}

use Data::Dumper;
sub debug
{
    my ($self, $what) = @_;

    if ($self->{debug}) {
        if (ref $what) {
            print STDERR Data::Dumper::Dumper($what);
        } else {
            print STDERR "$what\n";
        }
    }
}

use Time::ParseDate qw/parsedate/;

sub add_job
{
    my ($self, %elt) = @_;

    foreach my $d (@{$elt{data}}) {

	if ($d->{begin} !~ /^\d+$/) { # date => second
	    $d->{begin} = parsedate($d->{begin});
	}

	if ($d->{end} !~ /^\d+$/) { # date => second
	    $d->{end}   = parsedate($d->{end});
	}

        if ($self->{cbegin} == 0) {
            $self->{cbegin} = $d->{begin};
            $self->{cend} = $d->{end};
        }

        # calculate begin and end graph
        if ($self->{cbegin} > $d->{begin}) {
            $self->{cbegin}= $d->{begin};
        }
        if ($self->{cend} < $d->{end}) {
            $self->{cend} = $d->{end};
        }

        # TODO: check elt
    }

    push @{$self->{data}}, \%elt;
}

# si le label est identique, on reste sur la meme ligne
# sinon, on prend la ligne suivante
sub get_y
{
    my ($self, $label) = @_;

    unless ($self->{label_y}->{$label}) {
        $self->{label_y}->{$label} = $self->{cur_y};
        $self->{cur_y} += $self->{elt_h} + 5;
    }

    return $self->{label_y}->{$label} ;
}

sub get_color
{
    my ($self, $label) = @_;

    return 1 unless ($label);

    unless ($self->{type}->{$label}) {
        $self->{type}->{$label} = 1;
    }

    return $self->{type}->{$label} ;
}

sub draw_elt
{
    my ($self, $elt, $y1) = @_;
    use integer;

    my $y2 = $self->{height} - ($y1 + $self->{elt_h}) ;
    $y1 = $self->{height} - $y1;

    my $x1 = $self->{xmarge} + ($elt->{begin} - $self->{cbegin}) * $self->{width}/ $self->{period} ;
    my $x2 = $self->{xmarge} + ($elt->{end} - $self->{cbegin}) * $self->{width}/ $self->{period} ;

    my $color = $self->get_color($elt->{type});

    $gd->rectangle($x1,$y1,
                   $x2,$y2,
                   $black);
    if (($x1 + 4) <= $x2) {
	$gd->fill(($x1 + $x2)/2,
		  ($y1 + $y2)/2,
		  $color_tab[$color]);
    }
}

sub init_gd
{
    my ($self) = @_;
 
    unless (defined $gd) {
	my $height = $self->{height} ;
	my $width  = $self->{width} ;
    
	$gd    = new GD::Image($width+350,$height+200);
	$white = $gd->colorAllocate(255,255,255);
	
	push @color_tab, ($gd->colorAllocate(135,236,88),
			  $gd->colorAllocate( 255, 236, 139),
			  $gd->colorAllocate(255,95,95),
			  $gd->colorAllocate(255,149,0),
			  $gd->colorAllocate( 255, 174, 185),
			  $gd->colorAllocate( 179, 255,  58),
			  $gd->colorAllocate( 205, 133,   0),
			  $gd->colorAllocate(205, 133,   0 ),
			  $gd->colorAllocate(238, 238, 209),
			  ) ;
    
	$black = $gd->colorAllocate(0,0,0);
	#$gd->transparent($white);
        $gd->interlaced('true');
	#         x  y   x    y
	$gd->line($self->{xmarge},10, $self->{xmarge}, $height, $color_tab[1]);
	$gd->line($self->{xmarge},$height, $width, $height, $black);
    }
}
use POSIX qw/strftime/;

sub finalize
{
    my ($self) = @_;
    
    my $nb_elt = scalar(@{$self->{data}});
    #          (4 drive + nb job) * (size of elt)
    $self->{height} = (4+$nb_elt) * ($self->{elt_h} + 5);

    $self->init_gd();

    if ($self->{begin}) {
	$self->{cbegin} = $self->{begin};
    }
    if ($self->{end}) {
	$self->{cend} = $self->{end};
    }

    $self->{period} = $self->{cend} - $self->{cbegin};
    return if ($self->{period} < 1);

    foreach my $elt (@{$self->{data}}) {
        my $y1 = $self->get_y($elt->{label});

        $gd->string(GD::Font->Small,
                    1,          # x
                    $self->{height} - $y1 - 12,
                    $elt->{label}, $black);

        foreach my $d (@{$elt->{data}}) {
            $self->draw_elt($d, $y1);
        }
    }

    for my $i (0..10) {
	my $x1 = $i/10*$self->{width} + $self->{xmarge} ;
	$gd->stringUp(GD::Font->Small,
		      $x1,  # x
		      $self->{height} + 100, # y
		      strftime('%D %H:%M', localtime($i/10*$self->{period}+$self->{cbegin})), $black);

	$gd->dashedLine($x1,
			$self->{height} + 100,
			$x1,
			100,
			$black);
    }

    # affichage des legendes
    my $x1=100;
    my $y1=$self->{height} + 150;

    for my $t (keys %{$self->{type}}) {
	my $color = $self->get_color($t);

	$gd->rectangle($x1 - 5,
		       $y1 - 10,
		       $x1 + length($t)*10 - 5,
		       $y1 + 15,
		       $color_tab[$color]);
	
	$gd->fill($x1, $y1, $color_tab[$color]);

	$gd->string(GD::Font->Small,
		    $x1,
		    $y1,
		    $t,
		    $black);
	$x1 += length($t)*10;
    }

    #binmode STDOUT;
    #print $gd->png;
}


1;
__END__

package main ;

my $begin1 = "2006-10-03 09:59:34";
my $end1   = "2006-10-03 10:59:34";

my $begin2 = "2006-10-03 11:59:34";
my $end2 = "2006-10-03 13:59:34";

my $top = new GTime(debug => 1,
                    type => {
                        init =>  2,
                        write => 3,
                        commit => 4,
                    }) ;

$top->add_job(label => "label",
              data => [
                       {
                           type  => "init",
                           begin => $begin1,
                           end   => $end1,
                       },

                       {
                           type  => "write",
                           begin => $end1,
                           end   => $begin2,
                       },

                       {
                           type  => "commit",
                           begin => $begin2,
                           end   => $end2,
                       },
                       ]);

$top->add_job(label => "label2",
              data => [
                       {
                           type  => "init",
                           begin => $begin1,
                           end   => $end1,
                       },

                       {
                           type  => "write",
                           begin => $end1,
                           end   => $begin2,
                       },

                       {
                           type  => "commit",
                           begin => $begin2,
                           end   => $end2,
                       },
                       ]);

$top->finalize() ;
#$top->draw_labels() ;
# make sure we are writing to a binary stream
binmode STDOUT;

# Convert the image to PNG and print it on standard output
print $gd->png;
