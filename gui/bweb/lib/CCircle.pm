package CCircle ;

=head1 LICENSE

   Bweb - A Bacula web interface
   Bacula® - The Network Backup Solution

   Copyright (C) 2000-2006 Free Software Foundation Europe e.V.

   The main author of Bweb is Eric Bollengier.
   The main author of Bacula is Kern Sibbald, with contributions from
   many others, a complete list can be found in the file AUTHORS.

   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version two of the GNU General Public
   License as published by the Free Software Foundation plus additions
   that are listed in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.

   Bacula® is a registered trademark of John Walker.
   The licensor of Bacula is the Free Software Foundation Europe
   (FSFE), Fiduciary Program, Sumatrastrasse 25, 8006 Zurich,
   Switzerland, email:ftf@fsfeurope.org.

=head1 VERSION

    $Id$

=cut

use strict ;
use GD ;

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

my $debug = 0 ;
my $font_size = 6 ;

our @image_map ;

sub new
{
    my ($class, %arg) = @_ ;

    my $self = {
	start_degrees    => 0,
	degrees_complete => 0,
	parent_percent   => 100,
	level            => 1,
	center_x         => 600,
	center_y         => 300,
	diameter         => 75,
	percent_total    => 0,
	min_percent      => 2,
	min_total        => 0,
	width            => 1200,
	height           => 600,
	min_label        => 4,
	min_degrees      => 2,
	max_label_level  => 2,
	display_other    => 0,
	base_url         => '',
    } ;

    map { $self->{$_} = $arg{$_} } keys %arg ;

    unless(defined $gd) {
	$height = $self->{height} ;
	$width  = $self->{width} ;

	$gd    = new GD::Image($width,$height);
	$white = $gd->colorAllocate(255,255,255);

	push @color_tab, ($gd->colorAllocate(135,236,88),
			  $gd->colorAllocate(255,95,95),
			  $gd->colorAllocate(245,207,91),
			  $gd->colorAllocate( 255, 236, 139),
			  $gd->colorAllocate( 255, 174, 185),
			  $gd->colorAllocate( 179, 255,  58),
			  $gd->colorAllocate( 205, 133,   0),
			  $gd->colorAllocate(205, 133,   0 ),
			  $gd->colorAllocate(238, 238, 209),

			  ) ;
		      

	$black = $gd->colorAllocate(0,0,0);
	#$gd->>transparent($white);
	$gd->interlaced('true');

	$gd->arc($self->{center_x},$self->{center_y}, 
		 $self->{diameter},$self->{diameter},
		 $self->{start_degrees},360, $black) ;

    }

    $self->{rayon} = $self->{diameter} / 2 ;

    bless $self ;

    # pour afficher les labels tout propre
    if ($self->{level} > $last_level) {
	$last_level = $self->{level} ;
    }

    return $self ;
}

sub calc_xy
{
    my ($self, $level, $deg) = @_ ;

    my $x1 =   $self->{center_x}+$self->{rayon} * $level * cos($deg*$pi/180) ;
    my $y1 =   $self->{center_y}+$self->{rayon} * $level * sin($deg*$pi/180) ;

    return ($x1, $y1) ;

}

sub add_part
{
    my ($self, $percent, $label, $tips) = @_ ;

    $tips = $tips || $label ;

    if (($percent + $self->{percent_total}) > 100.05) {
	print STDERR "Attention $label ($percent\% + $self->{percent_total}\%) > 100\%\n" ;
	return undef; 
    }

    if ($percent <= 0) {
	print STDERR "Attention $label <= 0\%\n" ;
	return undef; 
    }

    # angle de depart de l'arc
    my $start_degrees = (($self->{degrees_complete})?
	                 $self->{degrees_complete}:$self->{start_degrees}) ;

    # angle de fin de l'arc
    my $end_degrees = $start_degrees +
	($percent * ( ( $self->{parent_percent} * 360 )/100 ) ) /100 ;

#    print STDERR "-------- $debug --------
#percent = $percent%
#label = $label
#level = $self->{level}
#start = $start_degrees
#end   = $end_degrees
#parent= $self->{parent_percent}
#" ;
    if (($end_degrees - $start_degrees) < $self->{min_degrees}) {
	$self->{min_total} += $percent ;
	return undef ;
    }

    if ($percent <= $self->{min_percent}) {
	$self->{min_total} += $percent ;
	return undef ;
    }

    # on totalise les % en cours
    $self->{percent_total} += $percent ;

    #print STDERR "percent_total = $self->{percent_total}\n" ;

    # position dans le cercle
    my $n = $self->{level} ; # on ajoute/retire 0.005 pour depasser un peu
    my $m = $n+1 ;

    # si c'est la premiere tranche de la nouvelle serie, il faut dessiner
    # la premiere limite 

    if ($self->{degrees_complete} == 0) {
	my ($x1, $y1) = $self->calc_xy($n-0.005, $self->{start_degrees}) ;
	my ($x2, $y2) = $self->calc_xy($m+0.005, $self->{start_degrees}) ;

	$gd->line($x1, $y1, $x2, $y2, $black) ;
    }

    # seconde ligne
    my ($x3, $y3) = $self->calc_xy($n-0.005, $end_degrees) ;

    my ($x4, $y4) = $self->calc_xy($m+0.005, $end_degrees) ;

    # ligne de bord exterieur
    $gd->line($x3, $y3, $x4, $y4, $black);

    # on dessine le bord
    $gd->arc($self->{center_x},$self->{center_y}, 
	     $self->{diameter}*($self->{level}+1),
	     $self->{diameter}*($self->{level}+1),

	     $start_degrees-0.5,
	     $end_degrees+0.5, $black) ;

    # on calcule le point qui est au milieu de la tranche
    # angle = (angle nouvelle tranche)/2

    # rayon = n*rayon - 0.5*rayon
    # n=1 -> 0.5
    # n=2 -> 1.5
    # n=3 -> 2.5

    my $mid_rad = ($end_degrees - $start_degrees) /2 + $start_degrees;

    my $moy_x = ($self->{center_x}+
		   ($self->{rayon}*$m - 0.5*$self->{rayon})
		       *cos($mid_rad*$pi/180)) ;

    my $moy_y = ($self->{center_y}+
		   ($self->{rayon}*$m - 0.5*$self->{rayon})
		       *sin($mid_rad*$pi/180)) ;

    $gd->fillToBorder($moy_x, 
		      $moy_y,
		      $black,
		      $cur_color) ;
    
    # on prend une couleur au hasard
    $cur_color = ($cur_color % $#color_tab) + 1 ;

    # si le % est assez grand, on affiche le label
    if ($percent > $self->{min_label}) {
	push @draw_label, [$label, 
			   $moy_x, $moy_y, 
			   $self->{level}] ;

	$self->push_image_map($label, $tips, $start_degrees, $end_degrees) ;
    }

    # pour pourvoir ajouter des sous donnees
    my $ret = new CCircle(start_degrees  => $start_degrees, 
			  parent_percent => $percent*$self->{parent_percent}/100,
			  level          => $m,
			  center_x       => $self->{center_x},
			  center_y       => $self->{center_y},
			  min_percent    => $self->{min_percent},
			  min_degrees    => $self->{min_degrees},
			  base_url       => $self->{base_url} . $label,
			  ) ;
		 
    $self->{degrees_complete} = $end_degrees ;
    
    #print STDERR "$debug : [$self->{level}] $label ($percent)\n" ;
    #open(FP, sprintf(">/tmp/img.%.3i.png", $debug)) ;
    #print FP $gd->png;
    #close(FP) ;
    
    $debug++ ;

    return $ret ;
}

# on dessine le restant < min_percent
sub finalize
{
    my ($self) = @_ ;

    $self->add_part($self->{min_total}, 
		    "other < $self->{min_percent}%",
		    $black) ; 

}

sub set_title
{
    my ($self, $title) = @_ ;

    $gd->string(GD::gdSmallFont, $self->{center_x} - $self->{rayon}*0.7, 
		$self->{center_y}, $title, $black) ;
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
    my ($self, $label, $tips, $start_degrees, $end_degrees) = @_ ;

    if ($label =~ /^other .*</) {
	if (!$self->{display_other}) {
	    return ;
	}
	$label = '';
    }

    # on prend des points tous les $delta sur l'arc interieur et exterieur

    my $delta = 3 ;

    if (($end_degrees - $start_degrees) < $delta) {
	return ;
    }

    my @pts ;

    for (my $i = $start_degrees ; 
	 $i <= $end_degrees ; 
	 $i = $i + $delta)
    {
	my ($x1, $y1) = $self->calc_xy($self->{level}, $i) ;
	my ($x2, $y2) = $self->calc_xy($self->{level} + 1, $i) ;

	push @pts, sprintf("%.2f,%.2f",$x1,$y1) ;
	unshift @pts, sprintf("%.2f,%.2f",$x2, $y2) ;
    }

    my ($x1, $y1) = $self->calc_xy($self->{level}, $end_degrees) ;
    my ($x2, $y2) = $self->calc_xy($self->{level} + 1, $end_degrees) ;

    push @pts, sprintf("%.2f,%.2f",$x1,$y1) ;
    unshift @pts, sprintf("%.2f,%.2f",$x2, $y2) ;

    my $ret = join(",", @pts) ;

    # on refait le traitement avec $i = $end_degrees
    $_image_map .= "<area shape='polygon' coords='$ret' ". 
	           "title='$tips' href='$self->{base_url}$label'>\n" ;

}

sub get_labels_imagemap
{
    my ($self) = @_ ;
    my @ret ;

    for my $l (@draw_label)
    {
	# translation 
	my ($label, $x, $y, $level) = @{ $l } ;
	
	next if ($level > $self->{max_label_level}) ;

	next if (!$self->{display_other} and $label =~ /^other .*</) ;

	my $dy = ($y - $self->{center_y})*($last_level - $level) + $y ;

	my $x2 ;
	my $xp ;

	if ($x < $self->{center_x}) {
	    $x2 =   $self->{center_x} 
	          - $self->{rayon} * ($last_level + 3.7) ;
	    $xp = $x2 - (length($label) *6 + 2) ; # moins la taille de la police 
	} else {
	    $x2 = $self->{center_x} + $self->{rayon} * ($last_level + 3.7) ;
	    $xp = $x2 + 10 ;
	}

	push @ret, $xp - 1 . ";" . $dy - 6 . ";" . $xp + length($label) * $font_size . ";" . $dy + 10 . "\n" ;

	$gd->rectangle($xp - 1, $dy - 6,
		       $xp + length($label) * $font_size, 
		       $dy + 10, $black) ;
    }
}

my $_label_hauteur ;
my $_label_max ;
my $_label_pos ;
my $_label_init = 0 ;

# on va stocker les positions dans un bitmap $_label_pos
# 
# si on match pas la position exacte, on essaye la case
# au dessus ou en dessous
# 
# on a un bitmap pour les labels de gauche et un pour la droite
# $_label_pos->[0] et $_label_pos->[1]
sub get_label_pos
{
    my ($self, $x, $y) = @_ ;

    unless ($_label_init) {
	$_label_hauteur   = $self->{rayon} * 2 * $last_level ;
	# nombre max de label = hauteur max / taille de la font
	$_label_max =  $_label_hauteur / 12 ;
	$_label_pos = [ [], [] ] ;
	$_label_init = 1 ;
    }

    # on calcule la position du label dans le bitmap
    use integer ;
    my $num = $y * $_label_max / $_label_hauteur ;
    no integer ;

    my $n = 0 ;			# nombre d'iteration
    my $l ;

    # on prend le bon bitmap
    if ($x < $self->{center_x}) {
	$l = $_label_pos->[0] ;
    } else {
	$l = $_label_pos->[1] ;
    }	    
    
    # on parcours le bitmap pour trouver la bonne position
    while (($num - $n) > 0) {
	if (not $l->[$num]) {
	    last ;
	} elsif (not $l->[$num + $n]) {
	    $num = $num + $n ;
	    last ;
	} elsif (not $l->[$num - $n]) {
	    $num = $num - $n ;
	    last ;
	} 
	$n++ ;
    }
    
    $l->[$num] = 1 ;		# on prend la position
    
    if ($num <= 0) {
	return 0 ;
    }
    
    # calcul de la position
    $y = $num * $_label_hauteur / $_label_max ;

    return $y ;
}

sub draw_labels
{
    my ($self) = @_ ;

    $gd->fillToBorder(1, 
		      1,
		      $black,
		      $white) ;

    for my $l (@draw_label)
    {
	# translation 
	my ($label, $x, $y, $level) = @{ $l } ;

	next if ($level > $self->{max_label_level}) ;

	next if (!$self->{display_other} and $label =~ /^other .*</) ;

	my $dx = ($x - $self->{center_x})*($last_level - $level) + $x ;
	my $dy = ($y - $self->{center_y})*($last_level - $level) + $y ;

	$dy = $self->get_label_pos($dx, $dy) ;

	next unless ($dy) ; # pas d'affichage si pas de place
	
	$gd->line( $x, $y,
		   $dx, $dy,
		   $black) ;

	my $x2 ;
	my $xp ;

	if ($x < $self->{center_x}) {
	    $x2 =   $self->{center_x} 
	          - $self->{rayon} * ($last_level + 3.7) ;
	    $xp = $x2 - (length($label) * $font_size + 2) ; # moins la taille de la police 
	} else {
	    $x2 = $self->{center_x} + $self->{rayon} * ($last_level + 3.7) ;
	    $xp = $x2 + 10 ;
	}

	$gd->line($dx, $dy,
		  $x2, $dy,
		  $black) ;

	$gd->string(GD::gdSmallFont, $xp, $dy - 5, $label, $black) ;
    }
}

1;
__END__

package main ;

my $top = new CCircle() ;

my $chld1 = $top->add_part(50, 'test') ;
my $chld2 = $top->add_part(20, 'test') ;
my $chld3 = $top->add_part(10, 'test') ;
my $chld4 = $top->add_part(20, 'test') ;


$chld1->add_part(20, 'test1') ;
$chld1->add_part(20, 'test1') ;

$chld2->add_part(20, 'test1') ;
$chld2->add_part(20, 'test1') ;

$chld3->add_part(20, 'test1') ;
my $chld5 = $chld3->add_part(20, 'test1') ;

$chld5->add_part(50, 'test3') ;

$top->finalize() ;
$chld1->finalize() ;
$chld2->finalize() ;
$chld3->finalize() ;
$chld4->finalize() ;
$chld5->finalize() ;

$top->draw_labels() ;
# make sure we are writing to a binary stream
binmode STDOUT;

# Convert the image to PNG and print it on standard output
print $CCircle::gd->png;
