;# ctime.pl is a simple Perl emulation for the well known ctime(3C) function.
;#
;# Waldemar Kebsch, Federal Republic of Germany, November 1988
;# kebsch.pad@nixpbe.UUCP
;# Modified March 1990, Feb 1991 to properly handle timezones
;#  $RCSfile: ctime.pl,v $$Revision: 4.0.1.1 $$Date: 92/06/08 13:38:06 $
;#   Marion Hakanson (hakanson@cse.ogi.edu)
;#   Oregon Graduate Institute of Science and Technology
;#
;# usage:
;#
;#     #include <ctime.pl>          # see the -P and -I option in perl.man
;#     $Date = &ctime(time);

CONFIG: {
    package ctime;

    @DoW = ('Sun','Mon','Tue','Wed','Thu','Fri','Sat');
    @MoY = ('Jan','Feb','Mar','Apr','May','Jun',
	    'Jul','Aug','Sep','Oct','Nov','Dec');
}

sub ctime {
    package ctime;

    local($time) = @_;
    local($[) = 0;
    local($sec, $min, $hour, $mday, $mon, $year, $wday, $yday, $isdst);

    # Determine what time zone is in effect.
    # Use GMT if TZ is defined as null, local time if TZ undefined.
    # There's no portable way to find the system default timezone.

    $TZ = defined($ENV{'TZ'}) ? ( $ENV{'TZ'} ? $ENV{'TZ'} : 'GMT' ) : '';
    ($sec, $min, $hour, $mday, $mon, $year, $wday, $yday, $isdst) =
        ($TZ eq 'GMT') ? gmtime($time) : localtime($time);

    # Hack to deal with 'PST8PDT' format of TZ
    # Note that this can't deal with all the esoteric forms, but it
    # does recognize the most common: [:]STDoff[DST[off][,rule]]

    if($TZ=~/^([^:\d+\-,]{3,})([+-]?\d{1,2}(:\d{1,2}){0,2})([^\d+\-,]{3,})?/){
        $TZ = $isdst ? $4 : $1;
    }
    $TZ .= ' ' unless $TZ eq '';

    $year += ($year < 70) ? 2000 : 1900;
    sprintf("%s %s %2d %2d:%02d:%02d %s%4d\n",
      $DoW[$wday], $MoY[$mon], $mday, $hour, $min, $sec, $TZ, $year);
}
1;
