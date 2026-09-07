find_program(ROOT_EXECUTABLE root HINTS /opt/ROOT/bin REQUIRED)

set(AT_MACROS ${CMAKE_CURRENT_SOURCE_DIR}/macros)
set(AT_CHECKS ${CMAKE_CURRENT_SOURCE_DIR})

# Runs the application on one macro in its own working directory, so that the
# output files of the different tests never collide.
function(acceptance_run test workdir macro)
  file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/${workdir})
  add_test(NAME ${test}
           COMMAND $<TARGET_FILE:gas-pressure-test> ${AT_MACROS}/${macro}
           WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/${workdir})
  set_tests_properties(${test} PROPERTIES LABELS acceptance)
endfunction()

# Judges the output of one or more runs with a ROOT macro.
function(acceptance_check test workdir depends call)
  add_test(NAME ${test}
           COMMAND ${ROOT_EXECUTABLE} -l -b -q "${AT_CHECKS}/${call}"
           WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/${workdir})
  set_tests_properties(${test} PROPERTIES LABELS acceptance DEPENDS "${depends}")
endfunction()

# --- AT-1 startup and file naming ------------------------------------------
acceptance_run(AT-1_default_run at1_default at1_default.mac)
acceptance_check(AT-1_default_check at1_default AT-1_default_run
                 [[check_events.C("Ar_200mbar_5.5MeV.root",10,0,"Ar",200)]])

acceptance_run(AT-1_hits_run at1_hits at1_hits.mac)
acceptance_check(AT-1_hits_check at1_hits AT-1_hits_run
                 [[check_events.C("Ar_200mbar_5.5MeV.root",10,1,"Ar",200)]])

acceptance_run(AT-1_custom_run at1_custom at1_custom.mac)
acceptance_check(AT-1_custom_check at1_custom AT-1_custom_run
                 [[check_events.C("custom.root",10,0,"Ar",200)]])

acceptance_run(AT-1_dotname_run at1_dotname at1_dotname.mac)
acceptance_check(AT-1_dotname_check at1_dotname AT-1_dotname_run
                 [[check_events.C("He_200mbar_0.3MeV.root",10,0,"He",200)]])

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/at1_noargs)
add_test(NAME AT-1_no_args
         COMMAND sh -c [=["$0"; test $? -eq 1 && test -z "$(ls *.root 2>/dev/null)"]=]
                 $<TARGET_FILE:gas-pressure-test>
         WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/at1_noargs)

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/at1_badgas)
add_test(NAME AT-1_bad_gas
         COMMAND sh -c [=[! "$0" "$1"]=]
                 $<TARGET_FILE:gas-pressure-test> ${AT_MACROS}/at1_badgas.mac
         WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/at1_badgas)

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/at1_badpressure)
add_test(NAME AT-1_bad_pressure
         COMMAND sh -c [=[! "$0" "$1"]=]
                 $<TARGET_FILE:gas-pressure-test> ${AT_MACROS}/at1_badpressure.mac
         WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/at1_badpressure)

set_tests_properties(AT-1_no_args AT-1_bad_gas AT-1_bad_pressure
                     PROPERTIES LABELS acceptance)

# --- AT-2 range at 1013.25 mbar, 5.5 MeV ------------------------------------
acceptance_run(AT-2_Ar_run at2_Ar at2_Ar.mac)
acceptance_check(AT-2_Ar_check at2_Ar AT-2_Ar_run
                 [[check_range.C("Ar_1013.25mbar_5.5MeV.root",45.2,3,44.3,3,true)]])

acceptance_run(AT-2_He_run at2_He at2_He.mac)
acceptance_check(AT-2_He_check at2_He AT-2_He_run
                 [[check_range.C("He_1013.25mbar_5.5MeV.root",235.9,3,235.2,3,true)]])

acceptance_run(AT-2_CO2_run at2_CO2 at2_CO2.mac)
acceptance_check(AT-2_CO2_check at2_CO2 AT-2_CO2_run
                 [[check_range.C("CO2_1013.25mbar_5.5MeV.root",27.5,3,27.3,3,true)]])

# --- AT-3 pressure scaling at 200 mbar, 5.5 MeV -----------------------------
acceptance_run(AT-3_Ar_run at3_Ar at3_Ar.mac)
acceptance_check(AT-3_Ar_check at3_Ar AT-3_Ar_run
                 [[check_range.C("Ar_200mbar_5.5MeV.root",228.9,3,224.3,3,true)]])

acceptance_run(AT-3_CO2_run at3_CO2 at3_CO2.mac)
acceptance_check(AT-3_CO2_check at3_CO2 AT-3_CO2_run
                 [[check_range.C("CO2_200mbar_5.5MeV.root",139.4,3,138.3,3,true)]])

# --- AT-4 low energy at 200 mbar, 0.3 MeV -----------------------------------
acceptance_run(AT-4_He_run at4_He at4_He.mac)
acceptance_check(AT-4_He_check at4_He AT-4_He_run
                 [[check_range.C("He_200mbar_0.3MeV.root",74.1,5,71.3,10,false)]])

acceptance_run(AT-4_Ar_run at4_Ar at4_Ar.mac)
acceptance_check(AT-4_Ar_check at4_Ar AT-4_Ar_run
                 [[check_range.C("Ar_200mbar_0.3MeV.root",16.7,5,12.9,10,false)]])

acceptance_run(AT-4_CO2_run at4_CO2 at4_CO2.mac)
acceptance_check(AT-4_CO2_check at4_CO2 AT-4_CO2_run
                 [[check_range.C("CO2_200mbar_0.3MeV.root",9.13,5,8.23,10,false)]])

# --- AT-5 punch through (He, 200 mbar, 5.5 MeV) -----------------------------
acceptance_run(AT-5_run at5_He at5_He.mac)
acceptance_check(AT-5_check at5_He AT-5_run
                 [[check_punchthrough.C("He_200mbar_5.5MeV.root",500,505,250,0.01,3.84,5)]])

# --- AT-6 energy conservation (Ar, 1013.25 mbar, hits) ----------------------
acceptance_run(AT-6_run at6 at6.mac)
acceptance_check(AT-6_check at6 AT-6_run
                 [[check_energy.C("Ar_1013.25mbar_5.5MeV.root")]])

# --- AT-7 reproducibility ---------------------------------------------------
acceptance_run(AT-7_run1 at7 at7_run1.mac)
acceptance_run(AT-7_run2 at7 at7_run2.mac)
acceptance_run(AT-7_run3 at7 at7_run3.mac)
acceptance_check(AT-7_same_check at7 "AT-7_run1;AT-7_run2"
                 [[check_repro.C("at7_run1.root","at7_run2.root",true)]])
acceptance_check(AT-7_seed_check at7 "AT-7_run1;AT-7_run3"
                 [[check_repro.C("at7_run1.root","at7_run3.root",false)]])

# --- AT-8 timing (Ar, 1013.25 mbar, hits) -----------------------------------
acceptance_run(AT-8_run at8 at8.mac)
acceptance_check(AT-8_check at8 AT-8_run
                 [[check_time.C("Ar_1013.25mbar_5.5MeV.root",2,20)]])

# --- AT-9 hit sanity (Ar, 200 mbar, hits) -----------------------------------
acceptance_run(AT-9_run at9 at9.mac)
acceptance_check(AT-9_check at9 AT-9_run [[check_hits.C("Ar_200mbar_5.5MeV.root",1.0)]])

# --- AT-10 hits must not change the physics ---------------------------------
acceptance_run(AT-10_false_run at10 at10_false.mac)
acceptance_run(AT-10_true_run at10 at10_true.mac)
acceptance_check(AT-10_check at10 "AT-10_false_run;AT-10_true_run"
                 [[check_meanclose.C("at10_false.root","at10_true.root",1)]])

# --- AT-11 GPS distributions are recorded -----------------------------------
acceptance_run(AT-11_lin_run at11_lin at11_lin.mac)
acceptance_check(AT-11_lin_check at11_lin AT-11_lin_run
                 [[check_gps_energy.C("Ar_200mbar_Lin.root",1,6,1)]])

acceptance_run(AT-11_iso_run at11_iso at11_iso.mac)
acceptance_check(AT-11_iso_check at11_iso AT-11_iso_run
                 [[check_gps_angle.C("Ar_200mbar_5.5MeV.root",0.98)]])
