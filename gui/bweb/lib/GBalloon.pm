
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

package GBalloon ;

our $gd ;
our @color_tab ;

our $black ;
our $white ;

sub new
{
    my ($class, %arg) = @_ ;

    my $self = {
        height     => 800,      # element height in px
        width      => 600,      # element width in px
        xmarge     => 75,
        ymarge     => 100,
	z_max_size => 50,       # max circle size
	z_min_size => 8,	# min circle size

	x_min => 0,
	y_min => 0,
	x_max => 1,
	y_max => 1,
	z_max => 1,

        data       => [],
        type       => {},

	img_id => 'imggraph',	# used to display graph

        base_url   => '',
    } ;

    map { $self->{$_} = $arg{$_} } keys %arg ;

    bless $self ;

    return $self ;
}

sub get_imagemap
{
    my ($self, $title, $img) = @_ ;

    return "
<map name='$self->{img_id}'>
" . join ('', reverse @{$self->{image_map}}) . "
</map>
<script type='text/javascript' language='JavaScript'>
 document.getElementById('$self->{img_id}').src='$img';
</script>
" ;

}

sub push_image_map
{
    my ($self, $label, $tips, $x, $y, $z) = @_ ;

    my $ret = sprintf("%.2f,%.2f,%.2f",
                      $x,$y,$z);

    push @{$self->{image_map}}, 
        "<area shape='circle' coords='$ret' ".
	"title='$tips' href='$self->{base_url}${label}'>\n" ;
}

sub init_gd
{
    my ($self) = @_;
 
    unless (defined $gd) {
	my $height = $self->{height} ;
	my $width  = $self->{width} ;
    
	$gd    = new GD::Image($width+100,$height+100);
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
	$gd->transparent($white);
#        $gd->interlaced('true');
	#         x  y   x    y
    }
}
use POSIX qw/strftime/;

sub compute
{
    my ($self, $d) = @_;

#    print STDERR "compute: x=$d->[0] y=$d->[1] z=$d->[2]\n";

    #       offset                       percent                 max
    my $x = $self->{xmarge} + $d->[0] / $self->{x_max} * ($self->{width} - $self->{xmarge});
    my $y = $self->{height} - $d->[1] / $self->{y_max} * ($self->{height} - $self->{ymarge});
    my $z = sqrt($d->[2]) / $self->{z_max} * $self->{z_max_size};

    if ($z < $self->{z_min_size}) {
	$z += $self->{z_min_size};	# min size
    }

    return ($x, $y, $z);
}

sub finalize
{
    my ($self) = @_;

    # we need to display big z before min z
    my @data = sort { $b->[2] <=> $a->[2] } @{$self->{data}};
    return unless (scalar(@data));

    # the max z will take something like 10% of the total size
    $self->{z_max} = sqrt($data[0]->[2]) || 1;
    my $c=0;

#   print STDERR "max: x=$self->{x_max} y=$self->{y_max} z=$self->{z_max}\n";

    foreach my $d (@data)
    {
	my ($x, $y, $z) = $self->compute($d);
#	print STDERR "current: x=$x y=$y z=$z\n";
	$c = ($c+1) % scalar(@color_tab);
	$gd->filledArc($x, $y, 2*$z, 2*$z, 0, 360, $color_tab[$c]);
	$self->push_image_map($d->[3], $d->[4],$x,$y,$z);
    }

    $self->draw_axis();
}

sub set_legend_axis
{
    my ($self, %arg) = @_;

    unless ($arg{x_func}) {
	$arg{x_func} = sub { join(" ", @_) }
    }

    unless ($arg{y_func}) {
	$arg{y_func} = sub { join(" ", @_) }
    }

    $self->{axis} = \%arg;
}

sub draw_axis
{
    my ($self) = @_;
    # draw axis
    $gd->line($self->{xmarge}, 5, 
	      $self->{xmarge}, $self->{height}, 
	      $black);
    $gd->line($self->{xmarge}, $self->{height},
	      $self->{width} - 5, $self->{height}, $black);

    $gd->string(GD::Font->Small,
		$self->{width} - 5,
		$self->{height} + 10,
		$self->{axis}->{x_title},
		$black);

    $gd->string(GD::Font->Small,
		0,
		10,
		$self->{axis}->{y_title},
		$black);

    my $h = $self->{height} ;
    my $w = $self->{width}  - $self->{xmarge};

    my $i=0;
    for (my $p = $self->{xmarge}; $p <= $w; $p = $p + $w/7) {
	$gd->string(GD::Font->Small,
		    $p,
		    $self->{height} + 10,
		    $self->{axis}->{x_func}($i),
		    $black);	

	$gd->line($p,
		  $self->{height} - 2,
		  $p,
		  $self->{height} + 2,
		  $black);	
   
	$i = $i + $self->{x_max}/7;
    }

    $i=0;
    for (my $p = $h; $p >= 0; $p = $p - $h/7) {
	$gd->string(GD::Font->Small,
		    10, $p, 
		    $self->{axis}->{y_func}($i),
		    $black);	

	$gd->line($self->{xmarge} - 2,
		  $p,
		  $self->{xmarge} + 2,
		  $p,
		  $black);	
   
	$i = $i + $self->{y_max}/7;
    }
}

sub add_point
{
    my ($self, $x_val, $y_val, $z_val, $label, $link) = @_;
    return unless ($x_val or $y_val or $z_val or $label);

    if ($self->{x_max} < $x_val) {
	$self->{x_max} = $x_val;
    }
    if ($self->{y_max} < $y_val) {
	$self->{y_max} = $y_val;
    }

    if ($self->{x_min} > $x_val) {
	$self->{x_min} = $x_val;
    }
    if ($self->{y_min} > $y_val) {
	$self->{y_min} = $y_val;
    }

    # z will be compute after
    push @{$self->{data}}, [$x_val, $y_val, $z_val, $label, $link];
}

1;
__END__

package main ;

# display Mb/Gb/Kb
sub human_size
{
    my @unit = qw(B KB MB GB TB);
    my $val = shift || 0;
    my $i=0;
    my $format = '%i %s';
    while ($val / 1024 > 1) {
        $i++;
        $val /= 1024;
    }
    $format = ($i>0)?'%0.1f %s':'%i %s';
    return sprintf($format, $val, $unit[$i]);
}

# display Day, Hour, Year
sub human_sec
{
    use integer;

    my $val = shift;
    $val /= 60;                 # sec -> min

    if ($val / 60 <= 1) {
        return "$val mins";
    }

    $val /= 60;                 # min -> hour
    if ($val / 24 <= 1) {
        return "$val hours";
    }

    $val /= 24;                 # hour -> day
    return "$val days";
}

my $top = new GBalloon(debug => 1) ;
$top->set_legend_axis(x_title => 'Time', x_func => \&human_sec,
		      y_title => 'Size', y_func => \&human_size,
		      z_title => 'Nb files');

$top->add_point( 13*60+56 , 3518454692 ,        6 , 'PRLISVCS_RMAN');
$top->add_point( 23*60+31 , 2419151398 ,        4 , 'RHREC_BACKUP' );
$top->add_point( 12*60+07 , 1373969860 ,     1044 , '3_DATA');
$top->add_point( 13*60+40 , 2284109956 ,     1121 , '4_DATA');
$top->add_point( 13*60+40 , 2284109956 ,     1 , '1_DATA');
$top->add_point( 13*60+10 , 2284109956 ,     1 , '2_DATA');
$top->add_point( 13*60+10 , 2284109956 ,     2000000 , '7_DATA');



$top->init_gd();
$top->finalize() ;
#$top->draw_labels() ;
# make sure we are writing to a binary stream
binmode STDOUT;

# Convert the image to PNG and print it on standard output
print $gd->png;

print STDERR "<html><body>";
print STDERR $top->get_imagemap("example", "a.png");
print STDERR "</body></html>";
