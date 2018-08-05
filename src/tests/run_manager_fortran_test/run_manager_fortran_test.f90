!  run_manager_fortran_test.f90 
!
!  FUNCTIONS:
!  run_manager_fortran_test - Entry point of console application.
!

!****************************************************************************
!
!  PROGRAM: run_manager_fortran_test
!
!  PURPOSE:  Entry point for the console application.
!
!****************************************************************************

    program run_manager_fortran_test

    implicit none
     ! Variables
     
    external rmif_create_serial
    integer rmif_create_serial
    external rmif_create_panther
    integer rmif_create_panther
    external rmif_create_genie
    integer rmif_create_genie
    external rmif_initialize
    integer rmif_initialize
    external rmif_reinitialize
    integer rmif_reinitialize
    external rmif_add_run
    integer rmif_add_run
    external rmif_run
    integer rmif_run
    external rmif_run_until
    integer rmif_run_until
    external rmif_get_run
    integer rmif_get_run
    external rmif_get_num_failed_runs
    integer rmif_get_num_failed_runs
    external rmif_get_failed_run_ids
    integer rmif_get_failed_run_ids
    external rmif_get_num_total_runs
    integer rmif_get_num_total_runs
    
    
    external rmif_delete
    integer rmif_delete
    
    character*20 comline(1)
    character*20 tpl(1)
    character*20 inp(1)
    character*20 ins(1)
    character*20 out(1)
    character*20 storfile
    character*20 port
    character*20 rmi_info_file
    character*80 rundir
    character*20 genie_host
    character*80 genie_tag
    character*20 p_names(3)
    character*50 o_names(16)
    character buf
    DOUBLE PRECISION pars(3)
    DOUBLE PRECISION bad_pars(3)
    DOUBLE PRECISION obs(16)
    integer nruns, npar, nobs
    integer itype
    integer err
    integer ipar, iobs, irun
    integer nfail
    integer failed_run_ids(100)
    integer n_total_runs
    
    integer run_input_flag
    integer run_output_flag
    integer run_until_no_ops
    DOUBLE PRECISION run_until_time_sec
    
    ! Body of run_manager_fortran_test
    data comline   /'storage1_win.exe  '/
    data tpl       /'input.tpl           '/
    data inp       /'input.dat           '/
    data ins       /'output.ins          '/
    data out       /'output.dat          '/
    data storfile  /'tmp_run_data.bin    '/
    !PANTHER parameters
    data rmi_info_file  /'run_manager_info.txt'/
    !serial run manager parameters
    data rundir    /'.\                  '/

    
    data p_names   /'recharge            ',&
                    'cond                ',&
                    'scoeff              '/
    
    data o_names   /'head1               ',&
                    'head2               ',&
                    'head3               ',&
                    'head4               ',&
                    'head5               ',&
                    'head6               ',&
                    'head7               ',&
                    'head8               ',&
                    'head9               ',&
                    'head10              ',&
                    'head11              ',&
                    'head12              ',&
                    'head13              ',&
                    'head14              ',&
                    'head15              ',&
                    'head16              '/
    
    data pars / 0.1, .005, .05/
    data bad_pars / 0.1, -9e9, .05/
    
    ! instantiate PANTHER run manager
    write(*,*) 'please enter port:'
    read(*,*) port
    err = rmif_create_panther(storfile, 20,&
            port, 20,&
            rmi_info_file, 20, 2, 1.15, 100.0)
            
    
    
    nruns = 4
    npar = 3
    nobs = 16
    ! intitialize run manager - allocate memory initialize parameter and observation names
    err = rmif_initialize(p_names, 20, npar, o_names, 50, nobs)
    
    ! add model runs to the queue
    err = rmif_add_run(bad_pars, npar, irun)
    do irun = 1, nruns - 1
        pars(1) = pars(1) + pars(1) * 0.2
        err = rmif_add_run(pars, npar, irun)
    end do
    
    ! perform mdoel runs
    write(*,*) 'Performing model runs...'
    ! set flag to return after 15sec
    run_input_flag = 2
    run_until_no_ops = 0
    run_until_time_sec = .05
    run_output_flag = -1
    !run_input_flag parameter is used specifiy when the run_until function should return
    !  run_input_flag = 0  -> return after all runs are complete
    !  run_input_flag = 1  -> return after "run_until_no_ops" consecutive loops in the internal run manager code have not had any tcp/ip action
    !  run_input_flag = 2  -> return after "run_until_time_sec" seconds
    !  run_input_flag = 3 ->  return after "run_until_no_ops" consecutive loops in the internal run manager code have not had any tcp/ip action 
    !                         or "run_until_time_sec" seconds.  Which ever comes first
    
    ! run_output_flag returns the reason the call to rmif_run_until is returning
    !  run_output_flag = 0 -> all runs are complete
    !  run_output_flag = 1 -> "run_until_no_ops" was active and this condition was satifisfied
    !  run_output_flag = 2 -> run_until_time_sec" was active and this condition occured
    do while (run_output_flag /= 0)
        err = rmif_run_until(run_input_flag, run_until_no_ops, run_until_time_sec, run_output_flag)
        write(*,*) 'Doing useful stuff while program runs'
        write(*,*) ''
    end do
    ! get number of failed model runs
    err = rmif_get_num_failed_runs(nfail)
    write(*,*) 'Number of failed runs: ', nfail
    err = rmif_get_failed_run_ids(failed_run_ids, 100)
    do irun = 1, nfail
         write(*,*) '   failed run id = ', failed_run_ids(irun)
    end do    
    
    ! read results
    do irun = 0, nruns-1
        err = rmif_get_run(irun, pars,npar, obs, nobs)
        
        write(*,*) ""
        write(*,*) ""
        write(*,*) "Results for model run", irun
        write(*,*) "Parameter Values:"
        do ipar = 1, npar
            write(*,*) "  parameter ", p_names(ipar), " = ", pars(ipar)
        end do
        if (err ==0 ) then
            write(*,*) "Observation Values:"
            do iobs = 1, nobs
                write(*,*) "  observation ", o_names(iobs), " = ", obs(iobs)
            end do
        else
            write(*,*) "run failed..."
        endif
    end do
    
    err = rmif_get_num_total_runs(n_total_runs)
    write(*,*) ''
    write(*,*) 'Total number of successful model runs:', n_total_runs
    
    ! reinitialize run manager and make another set of runs
    err = rmif_reinitialize()
    err = rmif_add_run(pars, npar, irun)
    pars(1) = pars(1) + pars(1) * 0.2
    err = rmif_add_run(pars, npar, irun)
    err = rmif_run()
    
    err = rmif_get_run(0, pars,npar, obs, nobs)
    
    err = rmif_get_num_total_runs(n_total_runs)
    write(*,*) ''
    write(*,*) 'Total number of successful model runs:', n_total_runs
    
    !clean up
    err = rmif_delete()
   
    write (*,*) ""
    write (*,*) "Press ENTER to continue"
    read (*,*)
    end program run_manager_fortran_test

