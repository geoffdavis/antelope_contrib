#
#   program needs:
#    DONE miniseed2days BH and LH data
#    DONE miniseed2days VH, UH, and SOH data into year files
#    DONE miniseed2db into one db
#    DONE attach dbmaster, dbops, and idserver
#    DONE verify if start and endtimes match deployment table
#
#    DONE idservers dbpath locking 
#    DONE net-sta chec check
#    DONE check to make sure correct channels are in file
#
#   check for overlaps
#
#
    require "getopts.pl" ;
    use strict ;
    use Datascope ;
    use archive;
    use timeslice ;
    use orb ;
    
    our ($pgm,$host);
    our ($opt_v,$opt_V,$opt_m,$opt_n,$opt_p);
    
{    #  Main program

    my ($usage,$cmd,$subject,$verbose,$debug,$Pf,$problems);
    my ($dbops,$stime,$table,$sta,$chan,$bhname,$sohname,$dirname,$dbname);
    my ($mintime,$maxtime,$etime,$mseedfile);
    my (@dbtest,@db,@dbwfdisc,@dbbh,@dbdeploy,@dbsnet,@dbschan,@dbsensor,@dbchanperf,@dbcd,@mseedfiles);
    my ($row,$nrows,$time,$endtime,$equip_install,$equip_remove);
    my ($snet,$fsta,$statmp,$fchan,$chantmp,$ref,$line,$st1,$st2,$dep,$prob,$prob_check);
    my (%pf,%staperf);

    $pgm = $0 ; 
    $pgm =~ s".*/"" ;
    elog_init($pgm, @ARGV);
    $cmd = "\n$0 @ARGV" ;
    
    if (  ! &Getopts('vVnm:p:') || @ARGV < 1 ) { 
        $usage  =  "\n\n\nUsage: $0  \n	[-v] [-V] [-n] \n" ;
        $usage .=  "	[-p pf] [-m mail_to]  \n" ;
        $usage .=  "	 sta [sta_1 sta_2 ...]\n\n"  ; 
        
        elog_notify($cmd) ; 
        elog_die ( $usage ) ; 
    }
    
    &savemail() if $opt_m ; 
    elog_notify($cmd) ; 
    $stime = strydtime(now());
    chop ($host = `uname -n` ) ;
    elog_notify ("\nstarting execution on	$host	$stime\n\n");

    $Pf         = $opt_p || $pgm ;
    
    
    %pf = getparam($Pf);

    $opt_v      = defined($opt_V) ? $opt_V : $opt_v ;    
    $verbose    = $opt_v;
    $debug      = $opt_V;
    
    if (system_check(0)) {
        $subject = "Problems - $pgm $host	Ran out of system resources";
        &sendmail($subject, $opt_m) if $opt_m ; 
        elog_die("\n$subject");
    }
    $problems = 0;


#   subset for unprocessed data

#
#  process all new stations
#

    $table = "wfdisc";
    foreach $sta (@ARGV) {
        $stime = strydtime(now());
        elog_notify ("\nstarting processing station $sta\n\n");
    
#
#  perform existance checks
#
        $prob_check = $problems ;
        $bhname  = "$pf{balerdirbase}\/$sta\/$pf{bhdata_dir}";
        $sohname = "$pf{balerdirbase}\/$sta\/$pf{sohdata_dir}";
        elog_notify("bhname	$bhname	sohname	$sohname") if $opt_v ;
        if (! -d $bhname ) {
            $problems++ ;
            elog_complain("\nProblem $problems
                           \n	directory $bhname does not exist
                           \n	Skipping to next station");
        }
                        
        if ( ! -d $sohname ) {
            $problems++ ;
            elog_complain("\nProblem $problems
                           \n	directory $sohname does not exist
                           \n	Skipping to next station");
        }
                        
        $dirname  = "$pf{archivebase}\/$sta";
#         if ( -d $dirname) {
#             $problems++ ;
#             elog_complain("\nProblem $problems
#                            \n	directory $dirname already exists!
#                            \n	Skipping to next station");        
#         }
        
        $dbname   = "$pf{archivebase}\/$sta\/$sta";
        elog_notify("dirname	$dirname	dbname	$dbname") if $opt_v ;
        if (-d $dbname || -e "$dbname.$table") {
            @dbtest = dbopen($dbname,"r") ;
            @dbtest = dblookup(@dbtest,0,"$table",0,0) ;
            if (dbquery(@dbtest,dbTABLE_PRESENT)) {
                $problems++ ;
                elog_complain("\nProblem $problems
                               \n	database $dbname.$table already exists!
                               \n	Skipping to next station") ;
            }
            dbclose(@dbtest);
        }
        
        next if ($prob_check != $problems);
#
#  make output directory
#
        makedir($dirname);
        chdir($dirname);
        elog_notify("\nChanged directory to $dirname") if $opt_v ;
        
#
#  build station-channel-day seed files from baler final seismic miniseed directory  
#

        opendir(DIR,$bhname);
        @mseedfiles = grep { /C.*\.bms.*/  } readdir(DIR);
        closedir(DIR);
        
        foreach $mseedfile (@mseedfiles) {
            $cmd  = "miniseed2days -c -U -m \".*_.*_[BL]H._.*\" ";
            $cmd .= "-v " if $opt_V;
            $cmd .= " - < $bhname/$mseedfile ";
            $cmd .= "> /tmp/tmp_$mseedfile\_$$ 2>&1 " unless $opt_V ;
        
            if  ( ! $opt_n ) {
                elog_notify("\n$cmd") if $opt_v ;        
                $problems = run($cmd,$problems) ;
            } else {
                elog_notify("\nskipping $cmd") if $opt_v ;
            } 
           unlink "/tmp/tmp_$mseedfile\_$$" unless $opt_V ;
        }
        
#
#  build station-channel-year seed files from baler final soh, VH, UH miniseed directory  
#

        opendir(DIR,$sohname);
        @mseedfiles = grep { /C.*\.bms.*/  } readdir(DIR);
        closedir(DIR);
                
        foreach $mseedfile (@mseedfiles) {
            $cmd  = "miniseed2days -c -U -r \".*_.*_[BHL]H[ZNE12]_.*\" -w %Y/%{net}_%{sta}_%{chan}.msd ";
            $cmd .= "-v " if $opt_V;
            $cmd .= " - < $sohname/$mseedfile ";
            $cmd .= "> /tmp/tmp_$mseedfile\_$$ 2>&1 " unless $opt_V ;
            
            if  ( ! $opt_n ) {
                elog_notify("\n$cmd") if $opt_v ;        
                $problems = run($cmd,$problems) ;
            } else {
                elog_notify("\nskipping $cmd") if $opt_v ;
            }
            unlink "/tmp/tmp_$mseedfile\_$$" unless $opt_V ;
        }

#
#  run miniseed2db for all baler data.  set  miniseed_segment_seconds=0 to remove one day wfdisc 
#  default in miniseed2db
#
        unlink($sta) if (-e $sta);
        
        open(TR,">trdefaults.pf");
        print TR "miniseed_segment_seconds 0\n";
        close(TR);
        
        $cmd  = "miniseed2db ";
        $cmd .= "-v " if $opt_V;
        $cmd .= "20\*/[0-3][0-9][0-9] $sta ";
        $cmd .= "> /tmp/tmp_miniseed2db\_$$ 2>&1 " unless $opt_V ;
        
        if  ( ! $opt_n ) {
            elog_notify("\n$cmd") if $opt_v ;        
            $problems = run($cmd,$problems) ;
        } else {
            elog_notify("\nskipping $cmd") if $opt_v ;
        } 
        
        $cmd  = "miniseed2db -T 0.001 ";
        $cmd .= "-v " if $opt_V;
        $cmd .= "20\*/\*.msd $sta ";
        $cmd .= "> /tmp/tmp_miniseed2db\_$$ 2>&1 " unless $opt_V ;
        
        if  ( ! $opt_n ) {
            elog_notify("\n$cmd") if $opt_v ;        
            $problems = run($cmd,$problems) ;
        } else {
            elog_notify("\nskipping $cmd") if $opt_v ;
        } 
        
        unlink("trdefaults.pf");

#
#  Clean up final station wfdsic
#
        
        $cmd = "dbsubset $sta.wfdisc \"$pf{wfclean}\" | dbdelete -";
        
        if  (! $opt_n ) {
            elog_notify("\n$cmd") if $opt_v ;        
            $problems = run($cmd,$problems) ;
        } else {
            elog_notify("\nskipping $cmd") if $opt_v ;
        }        


#
#  Check for anomolous net and sta
#
        $prob = 0;
        open(PROB,"> /tmp/prob_$sta\_$$");
        
        @db     = dbopen($sta,'r');
        @dbsnet = dblookup(@db,0,"snetsta",0,0);
        $nrows  = dbquery(@dbsnet,"dbRECORD_COUNT");
        
        if ($nrows > 1) {
            $line = "\nDatabase problem\n	$sta database has $nrows unique net-sta pairs";
            elog_complain($line);
            print PROB "$line\n";

            for ($row = 0; $row<$nrows; $row++) {
                $dbsnet[3] = $row;
                ($snet,$fsta,$statmp) = dbgetv(@dbsnet,"snet","fsta","sta");
                $line = "	snet	$snet	fsta	$fsta	sta	$statmp";
                elog_complain($line) ;
                print PROB "$line\n";
            }
            $prob++;
        }

        unlink("$sta");
        unlink("$sta.lastid");

#
#  Find start time and end times
#

        unless ($opt_n) {
            @dbwfdisc = dblookup(@db,0,"wfdisc",0,0);
            @dbbh     = dbsubset(@dbwfdisc,"sta =~/$sta/ && chan =~ /[BL]H[ZNE]/");
            $mintime  = dbex_eval(@dbbh,"min(time)");
            $maxtime  = dbex_eval(@dbbh,"max(endtime)");
        }
        dbclose(@db);
        
        $stime = strtime($mintime);
        $etime = strtime($maxtime);

#
#  Set up descriptor file
#
        
        &cssdescriptor ($sta,$pf{dbpath},$pf{dblocks},$pf{dbidserver}) unless $opt_n;

#
#  Check for anomolous channels
#
        
        $prob = unwanted_channels($sta,$prob) unless $opt_n;
#         @db       = dbopen($sta,'r');
#         @dbschan  = dblookup(@db,0,"schanloc",0,0);
#         @dbschan  = dbsubset(@dbschan,"chan !~ /UH./");
#         @dbsensor = dblookup(@db,0,"sensor",0,0);
#         @dbwfdisc = dblookup(@db,0,"wfdisc",0,0);
#         @dbschan  = dbjoin(@dbschan,@dbwfdisc);
#         @dbschan  = dbseparate(@dbschan,"schanloc");
#         @dbschan  = dbnojoin(@dbschan,@dbsensor);
#         $nrows    = dbquery(@dbschan,"dbRECORD_COUNT");
#         if ($nrows > 0) {
#             $line = "\nDatabase problem\n	$sta schanloc has $nrows channels which do not join with sensor table";
#             elog_complain($line);
#             print PROB "$line\n";
#             for ($row = 0; $row<$nrows; $row++) {
#                 $dbschan[3] = $row;
#                 ($statmp,$fchan,$chantmp) = dbgetv(@dbschan,"sta","fchan","chan");
#                 $line = "	sta	$statmp	fchan	$fchan	chan	$chantmp";
#                 elog_complain($line) ;
#                 print PROB "$line\n";
#                 $prob++;
#             }
#         }
#         dbclose(@db);
#         
#         unlink("$sta.snetsta");
#         unlink("$sta.schanloc");
        
#
#  identify gaps in baler seismic data
#

        $cmd  = "rt_daily_return ";
        $cmd .= "-v " if $debug ;
        $cmd .= "-t \"$stime\" -e \"$etime\" ";
        $cmd .= "-s \"sta =~/$sta/ && chan=~/[BL]H./\" $dbname $dbname";
        $cmd .= " >/tmp/$sta\_return_$$ 2>&1" unless $opt_V;

        if  (! $opt_n ) {
            elog_notify("\n$cmd") if $opt_v ;        
            $problems = run($cmd,$problems) ;
        } else {
            elog_notify("\nskipping $cmd") if $opt_v ;
        }
 
#
#  evaluate data return
#
        $prob = eval_data_return ($sta,$prob,$problems) unless $opt_n;       
#         elog_notify(" ");
#         $staperf{max_ave_perf}  = 0;
#         $staperf{max_nperfdays} = 0;
#         $staperf{max_datadays} = 0;
#         @db = dbopen($sta,"r");
#         @db = dblookup(@db,0,"chanperf",0,0);
#         @dbdeploy = dblookup(@db,0,"deployment",0,0);
#         @dbdeploy = dbsubset(@dbdeploy,"sta=~/$sta/");
#         $dbdeploy[3] = 0;
#         ($time,$endtime) = dbgetv(@dbdeploy,"time","endtime");
#         $staperf{deploy_days} = int($endtime/86400) - int($time/86400.) ;
# 
#         foreach $chan (qw( BHZ BHN BHE LHZ LHN LHE)) {
#             @dbchanperf                = dbsubset(@db,"chan =~ /$chan/");
#             @dbcd                      = dbsubset(@dbchanperf,"perf > 0.0");
#             $staperf{$chan}{days}      = dbquery(@dbcd,"dbRECORD_COUNT");
#             $staperf{max_datadays}     = $staperf{$chan}{days} if ($staperf{$chan}{days} > $staperf{max_datadays});
#             if ($staperf{$chan}{days} == 0.0) {
#                 $staperf{$chan}{ave_perf}  = 0.0;
#             } else {
#                 $staperf{$chan}{ave_perf}  = (dbex_eval(@dbchanperf,"sum(perf)"))/$staperf{$chan}{days};
#             }
#             $staperf{max_ave_perf}     = $staperf{$chan}{ave_perf} if ($staperf{$chan}{ave_perf} > $staperf{max_ave_perf});
#             @dbchanperf                = dbsubset(@dbchanperf,"perf == 100.");
#             $staperf{$chan}{nperfdays} = dbquery(@dbchanperf,"dbRECORD_COUNT");
#             $staperf{max_nperfdays}    = $staperf{$chan}{nperfdays} if ($staperf{$chan}{nperfdays} > $staperf{max_nperfdays}) ;
#             $line = sprintf("%s  %s	%4d days with 100%% data return	with %5.1f%% average daily data return on days with data",
#                                 $sta,$chan,$staperf{$chan}{nperfdays},$staperf{$chan}{ave_perf});
#             elog_notify("	$line");
#             print PROB "$line\n" ;
#         }
#         dbclose(@db);
#         
#         $staperf{deploy_days} = 0.1 if ($staperf{deploy_days} < 0.1) ;
#         
#         $line = sprintf("%s	%4d deployment days	%4d days with data return	%5.1f%% of possible days",
#                          $sta,$staperf{deploy_days},$staperf{max_datadays},(100*$staperf{max_datadays}/$staperf{deploy_days}));
#         
#         if ($staperf{max_ave_perf} < 99.) {
#             $prob++;
#             print PROB "\n$line\n" ;
#             $problems++ ;
#             elog_complain("\nProblem $problems
#                            \n	$line");
#         } else {
#             elog_notify("\n	$line\n\n");
#         }
#         
#         %staperf = (); 
        
        close(PROB);

 
#
#  clean up
#
        
        $subject = "TA $sta baler data net, station, channel problem";
        $cmd     = "rtmail -C -s '$subject' $pf{prob_mail} < /tmp/prob_$sta\_$$";
        
        if  ( ! $opt_n ) {
            elog_notify("\n$cmd") if $prob ;        
            $problems = run($cmd,$problems) if $prob ;

        } else {
            elog_notify("\nskipping $cmd") if $prob ;
        } 
        
        $cmd  = "mv $pf{balerdirbase}\/$sta $pf{balerprocbase}";
        if  ( ! $opt_n && ! $prob) {
            elog_notify("\n$cmd") ;        
            $problems = run($cmd,$problems) ;

        } else {
            elog_notify("\nskipping $cmd") ;
        } 


        unlink "/tmp/$sta\_return_$$" unless $opt_V;
        unlink "/tmp/tmp_miniseed2db\_$$" unless $opt_V;
        unlink "/tmp/prob_$sta\_$$" unless $opt_V;
        
    }
        
    $stime = strydtime(now());
    elog_notify ("completed successfully	$stime\n\n");

    if ($problems == 0 ) {
        $subject = sprintf("Success  $pgm  $host");
        elog_notify ($subject);
        &sendmail ( $subject, $opt_m ) if $opt_m ;
    } else { 
        $subject = "Problems - $pgm $host	$problems problems" ;
        &sendmail($subject, $opt_m) if $opt_m ; 
        elog_die("\n$subject") ;
    }
  
    
    exit(0);
}

sub getparam { # %pf = getparam($Pf);
    my ($Pf) = @_ ;
    my ($subject);
    my (%pf) ;
    
    $pf{balerdirbase}		= pfget( $Pf, "balerdirbase" );
    $pf{balerprocbase}		= pfget( $Pf, "balerprocbase" );
    $pf{archivebase}		= pfget( $Pf, "archivebase" );
    $pf{bhdata_dir}			= pfget( $Pf, "bhdata_dir" );
    $pf{sohdata_dir}		= pfget( $Pf, "sohdata_dir" );
    
    $pf{dbpath}     		= pfget( $Pf, "dbpath" );
    $pf{dbidserver} 		= pfget( $Pf, "dbidserver" );
    $pf{dblocks}    		= pfget( $Pf, "dblocks" );
    
    $pf{wfclean}     		= pfget( $Pf, "wfclean" );

    $pf{prob_mail}    		= pfget( $Pf, "prob_mail" );
    
    if ($opt_V) {
        elog_notify("\nbalerdirbase     $pf{balerdirbase}");
        elog_notify("balerprocbase    $pf{balerprocbase}");
        elog_notify("archivebase      $pf{archivebase}");
        elog_notify("bhdata_dir       $pf{bhdata_dir}");
        elog_notify("sohdata_dir      $pf{sohdata_dir}");
        elog_notify("dbpath           $pf{dbpath}" );
        elog_notify("dbidserver       $pf{dbidserver}" );
        elog_notify("dblocks          $pf{dblocks}\n\n" );
        elog_notify("wfclean          $pf{wfclean}" );
        elog_notify("prob_mail        $pf{prob_mail}\n\n" );
    }
            
    return (%pf) ;
}

sub unwanted_channels { # $prob = unwanted_channels($sta,$prob);
    my ($sta,$prob) = @_ ; 
    my ($nrows,$row,$line,$statmp,$fchan,$chantmp) ;
    my (@db,@dbschan,@dbsensor,@dbwfdisc);
        
    @db       = dbopen($sta,'r');
    @dbschan  = dblookup(@db,0,"schanloc",0,0);
    @dbschan  = dbsubset(@dbschan,"chan !~ /UH./");
    @dbsensor = dblookup(@db,0,"sensor",0,0);
    @dbwfdisc = dblookup(@db,0,"wfdisc",0,0);
    @dbschan  = dbjoin(@dbschan,@dbwfdisc);
    @dbschan  = dbseparate(@dbschan,"schanloc");
    @dbschan  = dbnojoin(@dbschan,@dbsensor);
    $nrows    = dbquery(@dbschan,"dbRECORD_COUNT");
    if ($nrows > 0) {
        $line = "\nDatabase problem\n	$sta schanloc has $nrows channels which do not join with sensor table";
        elog_complain($line);
        print PROB "$line\n";
        for ($row = 0; $row<$nrows; $row++) {
            $dbschan[3] = $row;
            ($statmp,$fchan,$chantmp) = dbgetv(@dbschan,"sta","fchan","chan");
            $line = "	sta	$statmp	fchan	$fchan	chan	$chantmp";
            elog_complain($line) ;
            print PROB "$line\n";
            $prob++;
        }
    }
    dbclose(@db);
        
    unlink("$sta.snetsta");
    unlink("$sta.schanloc");
    return ($prob);

}

sub eval_data_return { # $prob = eval_data_return ($sta,$prob,$problems) ;
    my ($sta,$prob,$problems) = @_ ;
    my ($time,$endtime,$chan,$line) ;
    my (@db,@dbdeploy,@dbchanperf,@dbcd) ;
    
    my (%staperf);
    
    elog_notify(" ");
    $staperf{max_ave_perf}  = 0;
    $staperf{max_nperfdays} = 0;
    $staperf{max_datadays} = 0;
    @db = dbopen($sta,"r");
    @db = dblookup(@db,0,"chanperf",0,0);
    @dbdeploy = dblookup(@db,0,"deployment",0,0);
    @dbdeploy = dbsubset(@dbdeploy,"sta=~/$sta/");
    $dbdeploy[3] = 0;
    ($time,$endtime) = dbgetv(@dbdeploy,"time","endtime");
    $staperf{deploy_days} = int($endtime/86400) - int($time/86400.) ;

    foreach $chan (qw( BHZ BHN BHE LHZ LHN LHE)) {
        @dbchanperf                = dbsubset(@db,"chan =~ /$chan/");
        @dbcd                      = dbsubset(@dbchanperf,"perf > 0.0");
        $staperf{$chan}{days}      = dbquery(@dbcd,"dbRECORD_COUNT");
        $staperf{max_datadays}     = $staperf{$chan}{days} if ($staperf{$chan}{days} > $staperf{max_datadays});
        if ($staperf{$chan}{days} == 0.0) {
            $staperf{$chan}{ave_perf}  = 0.0;
        } else {
            $staperf{$chan}{ave_perf}  = (dbex_eval(@dbchanperf,"sum(perf)"))/$staperf{$chan}{days};
        }
        $staperf{max_ave_perf}     = $staperf{$chan}{ave_perf} if ($staperf{$chan}{ave_perf} > $staperf{max_ave_perf});
        @dbchanperf                = dbsubset(@dbchanperf,"perf == 100.");
        $staperf{$chan}{nperfdays} = dbquery(@dbchanperf,"dbRECORD_COUNT");
        $staperf{max_nperfdays}    = $staperf{$chan}{nperfdays} if ($staperf{$chan}{nperfdays} > $staperf{max_nperfdays}) ;
        $line = sprintf("%s  %s	%4d days with 100%% data return	with %5.1f%% average daily data return on days with data",
                         $sta,$chan,$staperf{$chan}{nperfdays},$staperf{$chan}{ave_perf});
        elog_notify("	$line");
        print PROB "$line\n" ;
    }
    dbclose(@db);
        
    $staperf{deploy_days} = 0.1 if ($staperf{deploy_days} < 0.1) ;
        
    $line = sprintf("%s	%4d deployment days	%4d days with data return	%5.1f%% of possible days",
                     $sta,$staperf{deploy_days},$staperf{max_datadays},(100*$staperf{max_datadays}/$staperf{deploy_days}));
        
    if ($staperf{max_ave_perf} < 99.) {
        $prob++;
        print PROB "\n$line\n" ;
        $problems++ ;
        elog_complain("\nProblem $problems
                       \n	$line");
    } else {
        elog_notify("\n	$line\n\n");
    }
        
    %staperf = (); 
    return ($prob,$problems);
}