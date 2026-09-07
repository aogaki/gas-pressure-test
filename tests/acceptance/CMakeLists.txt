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

# --- AT-M2 gas mixtures at 1013.25 mbar, 5.5 MeV (TODO/05) ------------------
# Expectations from NIST ASTAR with the range additivity rule 1/R = sum(w_i/R_i)
# applied to the mass ranges, both for the path length and for the projected
# range. The automatic file name carries the mixture specification.
acceptance_run(AT-M2_HeCO2_run atM2_HeCO2 atM2_HeCO2.mac)
acceptance_check(AT-M2_HeCO2_check atM2_HeCO2 AT-M2_HeCO2_run
                 [[check_range.C("He-90-CO2-10_1013.25mbar_5.5MeV.root",134.2,5,133.5,5,false)]])

acceptance_run(AT-M2_ArCO2_run atM2_ArCO2 atM2_ArCO2.mac)
acceptance_check(AT-M2_ArCO2_check atM2_ArCO2 AT-M2_ArCO2_run
                 [[check_range.C("Ar-90-CO2-10_1013.25mbar_5.5MeV.root",42.4,5,41.7,5,false)]])

# --- AT-S1 scan.sh dry run: macros are generated, nothing executes ---------
set(SCAN_SH ${PROJECT_SOURCE_DIR}/scripts/scan.sh)
set(RANGE_SUMMARY ${PROJECT_SOURCE_DIR}/scripts/range_summary.C)

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/atS1)
add_test(NAME AT-S1_run
         COMMAND ${SCAN_SH} -d -o . -g "He" -p "200" -e "0.3 1.0" -n 10
                 -x $<TARGET_FILE:gas-pressure-test>
         WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/atS1)
add_test(NAME AT-S1_check
         COMMAND sh -c [=[
set -e
diff - macros/He_200mbar_0.3MeV.mac <<'MAC'
/tpc/gas He
/tpc/pressure 200
/tpc/hits false
/run/initialize
/gps/energy 0.3 MeV
/analysis/setFileName He_200mbar_0.3MeV.root
/random/setSeeds 1 1
/run/beamOn 10
MAC
diff - macros/He_200mbar_1.0MeV.mac <<'MAC'
/tpc/gas He
/tpc/pressure 200
/tpc/hits false
/run/initialize
/gps/energy 1.0 MeV
/analysis/setFileName He_200mbar_1.0MeV.root
/random/setSeeds 2 2
/run/beamOn 10
MAC
test -z "$(ls *.root 2>/dev/null)"
]=]
         WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/atS1)
set_tests_properties(AT-S1_run PROPERTIES LABELS acceptance)
set_tests_properties(AT-S1_check PROPERTIES LABELS acceptance DEPENDS AT-S1_run)

# --- AT-S2 scan.sh real run, -j 2 -------------------------------------------
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/atS2)
add_test(NAME AT-S2_run
         COMMAND ${SCAN_SH} -o . -g "He" -p "200" -e "0.3 1.0" -n 10 -j 2
                 -x $<TARGET_FILE:gas-pressure-test>
         WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/atS2)
set_tests_properties(AT-S2_run PROPERTIES LABELS acceptance)

acceptance_check(AT-S2_check_0.3MeV atS2 AT-S2_run
                 [[check_scan_run.C("He_200mbar_0.3MeV.root",10,200)]])
acceptance_check(AT-S2_check_1.0MeV atS2 AT-S2_run
                 [[check_scan_run.C("He_200mbar_1.0MeV.root",10,200)]])

add_test(NAME AT-S2_logs
         COMMAND sh -c [=[test "$(ls logs/*.log 2>/dev/null | wc -l | tr -d ' ')" = 2]=]
         WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/atS2)
set_tests_properties(AT-S2_logs PROPERTIES LABELS acceptance DEPENDS AT-S2_run)

# --- AT-S3 range_summary.C over AT-S2's output ------------------------------
set(AT_S3_CALL "${RANGE_SUMMARY}(\".\")")
add_test(NAME AT-S3_run
         COMMAND sh -c [=["$0" -l -b -q "$1" > summary.csv]=]
                 ${ROOT_EXECUTABLE} "${AT_S3_CALL}"
         WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/atS2)
set_tests_properties(AT-S3_run PROPERTIES LABELS acceptance DEPENDS AT-S2_run)

acceptance_check(AT-S3_check atS2 AT-S3_run [[check_scan_summary.C("summary.csv")]])

# --- AT-S4 scan.sh with a non-existent executable exits non-zero -----------
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/atS4)
add_test(NAME AT-S4_missing_exe
         COMMAND sh -c [=[! "$0" -o . -g "He" -p "200" -e "0.3" -n 1 -x /no/such/exe-does-not-exist]=]
                 ${SCAN_SH}
         WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/atS4)
set_tests_properties(AT-S4_missing_exe PROPERTIES LABELS acceptance)

# ============================================================================
# Stage 2: drift-electrons (TODO/03)
# ============================================================================

# One Magboltz table for the whole stage. Making it takes about a minute, so
# AT2-4 (the cache test) builds it and AT2-2 and AT2-3 read it back.
set(S2_CACHE ${CMAKE_CURRENT_BINARY_DIR}/gascache)

add_executable(drift_one_check drift_one_check.cc)
target_link_libraries(drift_one_check PRIVATE drift_core ROOT::RIO ROOT::Tree)
target_include_directories(drift_one_check PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# --- AT2-1 CLI and output name ----------------------------------------------
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/s2_cli)
add_test(NAME AT2-1_no_input
         COMMAND sh -c [=[! "$0" -v 2000]=] $<TARGET_FILE:drift-electrons>
         WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/s2_cli)
add_test(NAME AT2-1_no_voltage
         COMMAND sh -c [=[! "$0" -i in.root]=] $<TARGET_FILE:drift-electrons>
         WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/s2_cli)
set_tests_properties(AT2-1_no_input AT2-1_no_voltage PROPERTIES LABELS acceptance)

acceptance_run(AT2-1_nohits_run s2_nohits drift_nohits.mac)
add_test(NAME AT2-1_no_hits
         COMMAND sh -c
                 [=[! "$0" -i Ar_200mbar_5.5MeV.root -v 2000 -c "$1" && test -z "$(ls *_2000V.root 2>/dev/null)"]=]
                 $<TARGET_FILE:drift-electrons> ${S2_CACHE}
         WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/s2_nohits)
set_tests_properties(AT2-1_no_hits PROPERTIES LABELS acceptance
                     DEPENDS AT2-1_nohits_run)

# --- AT2-4 the gas table cache ----------------------------------------------
# Runs twice on one event with almost no electrons: the first run makes the
# table, the second one has to be much faster because it only reads it.
acceptance_run(AT2-4_stage1 s2_cache drift_stage1.mac)
add_test(NAME AT2-4_cache
         COMMAND sh -c [=[
set -e
rm -rf "$1"
start=$(date +%s)
"$0" -i Ar_200mbar_5.5MeV.root -v 2000 -f 0.001 -n 1 -c "$1" > generate.log 2>&1
middle=$(date +%s)
"$0" -i Ar_200mbar_5.5MeV.root -v 2000 -f 0.001 -n 1 -c "$1" > reuse.log 2>&1
end=$(date +%s)
first=$((middle - start))
second=$((end - middle))
echo "first run ${first} s, second run ${second} s"
test -f "$1/Ar_200mbar_100Vcm.gas"
grep -q "running Magboltz" generate.log
grep -q "reading the gas table" reuse.log
! grep -q "running Magboltz" reuse.log
test "$second" -lt "$first"
]=]
                 $<TARGET_FILE:drift-electrons> ${S2_CACHE}
         WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/s2_cache)
set_tests_properties(AT2-4_cache PROPERTIES LABELS acceptance
                     DEPENDS AT2-4_stage1 TIMEOUT 900)

add_test(NAME AT2-1_output_name
         COMMAND sh -c [=[test -f Ar_200mbar_5.5MeV_2000V.root]=]
         WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/s2_cache)
set_tests_properties(AT2-1_output_name PROPERTIES LABELS acceptance
                     DEPENDS AT2-4_cache)

# --- AT2-2 one electron at a time -------------------------------------------
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/s2_one)
add_test(NAME AT2-2_drift COMMAND drift_one_check ${S2_CACHE}
         WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/s2_one)
set_tests_properties(AT2-2_drift PROPERTIES LABELS acceptance
                     DEPENDS AT2-4_cache TIMEOUT 900)

# --- AT2-3 end to end -------------------------------------------------------
acceptance_run(AT2-3_stage1 s2_end2end drift_stage1.mac)
add_test(NAME AT2-3_run
         COMMAND $<TARGET_FILE:drift-electrons>
                 -i Ar_200mbar_5.5MeV.root -v 2000 -f 0.1 -c ${S2_CACHE}
         WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/s2_end2end)
set_tests_properties(AT2-3_run PROPERTIES LABELS acceptance
                     DEPENDS "AT2-3_stage1;AT2-4_cache" TIMEOUT 900)
acceptance_check(AT2-3_check s2_end2end AT2-3_run
                 [[check_drift.C("Ar_200mbar_5.5MeV_2000V.root","Ar",200,2000,10,0.9)]])

# --- AT-M3 a gas mixture all the way through Stage 2 (TODO/05) --------------
# This adds a second Magboltz table (He/CO2 90/10) to the shared cache, which
# takes about 90 s. It has to wait for AT2-4, which empties the cache
# directory before it makes the argon table.
acceptance_run(AT-M3_stage1 s2_mixture atM3_stage1.mac)
add_test(NAME AT-M3_run
         COMMAND $<TARGET_FILE:drift-electrons>
                 -i He-90-CO2-10_200mbar_0.3MeV.root -v 2000 -f 0.1
                 -c ${S2_CACHE}
         WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/s2_mixture)
set_tests_properties(AT-M3_run PROPERTIES LABELS acceptance
                     DEPENDS "AT-M3_stage1;AT2-4_cache" TIMEOUT 900)
acceptance_check(AT-M3_check s2_mixture AT-M3_run
                 [[check_drift.C("He-90-CO2-10_200mbar_0.3MeV_2000V.root","He-90-CO2-10",200,2000,10,0.9,40.47,0.5,0.185,3)]])

# --- AT-D1 scan.sh -H asks Stage 1 for the hits ntuple ----------------------
# The "-H is missing" case is AT-S1, whose macros carry /tpc/hits false.
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/atD1)
add_test(NAME AT-D1_run
         COMMAND ${SCAN_SH} -H -d -o . -g "He" -p "200" -e "0.3" -n 10
                 -x $<TARGET_FILE:gas-pressure-test>
         WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/atD1)
add_test(NAME AT-D1_check
         COMMAND sh -c [=[
set -e
diff - macros/He_200mbar_0.3MeV.mac <<'MAC'
/tpc/gas He
/tpc/pressure 200
/tpc/hits true
/run/initialize
/gps/energy 0.3 MeV
/analysis/setFileName He_200mbar_0.3MeV.root
/random/setSeeds 1 1
/run/beamOn 10
MAC
test -z "$(ls *.root 2>/dev/null)"
]=]
         WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/atD1)
set_tests_properties(AT-D1_run PROPERTIES LABELS acceptance)
set_tests_properties(AT-D1_check PROPERTIES LABELS acceptance DEPENDS AT-D1_run)

# --- AT-D2 drift_scan.sh ----------------------------------------------------
set(DRIFT_SCAN_SH ${PROJECT_SOURCE_DIR}/scripts/drift_scan.sh)

# Dry run: one command per (Stage 1 file, voltage), and nothing is executed.
# The Stage 2 output of the run below is skipped because its name ends in "V",
# the Stage 3 output because it ends in "_readout".
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/atD2_dry)
add_test(NAME AT-D2_dry
         COMMAND sh -c [=[
set -e
rm -f ./*.root
touch Ar_200mbar_5.5MeV.root Ar_200mbar_5.5MeV_1000V.root \
      Ar_200mbar_5.5MeV_1000V_readout.root
out=$("$0" -d -o . -v "1000 2000" -x /no/such/exe 2>&1 >/dev/null)
printf '%s\n' "$out"
# One "(cd . && ..." line per (Stage 1 file, voltage); the _1000V.root file is
# a Stage 2 output and the _readout.root file a Stage 3 one, so neither may be
# picked up again.
test "$(printf '%s\n' "$out" | grep -c '^(cd ')" = 2
test "$(printf '%s\n' "$out" | grep -c -- '-v 2000')" = 1
test "$(printf '%s\n' "$out" | grep -c -- 'readout')" = 0
test ! -f Ar_200mbar_5.5MeV_2000V.root
]=]
                 ${DRIFT_SCAN_SH}
         WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/atD2_dry)
set_tests_properties(AT-D2_dry PROPERTIES LABELS acceptance)

# Real run on one Stage 1 file, reusing the argon table AT2-4 has cached.
acceptance_run(AT-D2_stage1 atD2 drift_stage1.mac)
add_test(NAME AT-D2_run
         COMMAND ${DRIFT_SCAN_SH} -o . -v "2000" -f 0.01 -j 2 -c ${S2_CACHE}
                 -x $<TARGET_FILE:drift-electrons>
         WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/atD2)
set_tests_properties(AT-D2_run PROPERTIES LABELS acceptance
                     DEPENDS "AT-D2_stage1;AT2-4_cache" TIMEOUT 900)
acceptance_check(AT-D2_check atD2 AT-D2_run
                 [[check_drift.C("Ar_200mbar_5.5MeV_2000V.root","Ar",200,2000,10,0.9)]])

add_test(NAME AT-D2_log
         COMMAND sh -c [=[test -s logs/Ar_200mbar_5.5MeV_2000V.log]=]
         WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/atD2)
set_tests_properties(AT-D2_log PROPERTIES LABELS acceptance DEPENDS AT-D2_run)

# ============================================================================
# Stage 4: aget-shaper, the tests that need no pad map (TODO/07)
# ============================================================================

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/s4_cli)
add_test(NAME AT4-1_no_input
         COMMAND sh -c [=[! "$0"]=] $<TARGET_FILE:aget-shaper>
         WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/s4_cli)
set_tests_properties(AT4-1_no_input PROPERTIES LABELS acceptance)

# A Stage 2 output has no "waveforms" ntuple, so Stage 4 has to refuse it.
add_test(NAME AT4-1_no_waveforms
         COMMAND sh -c [=[
set -e
cp ../s2_cache/Ar_200mbar_5.5MeV_2000V.root .
! "$0" -i Ar_200mbar_5.5MeV_2000V.root
test -z "$(ls *_raw.root 2>/dev/null)"
]=]
                 $<TARGET_FILE:aget-shaper>
         WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/s4_cli)
set_tests_properties(AT4-1_no_waveforms PROPERTIES LABELS acceptance
                     DEPENDS AT2-4_cache)

# ============================================================================
# Stage 3 (TODO/06) and Stage 4 (TODO/07): everything that needs the board
#
# geometry/pads.csv is derived from the real detector geometry and is not in
# git, so these tests exist only when it does; the root CMakeLists warns when
# it is missing. The block below is guarded, not indented, to keep it
# readable next to the stages above.
# ============================================================================
if(EXISTS ${PADS_CSV})

# --- AT3-1 CLI --------------------------------------------------------------
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/s3_cli)
add_test(NAME AT3-1_no_input
         COMMAND sh -c [=[! "$0" -p "$1"]=]
                 $<TARGET_FILE:channel-response> ${PADS_CSV}
         WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/s3_cli)
add_test(NAME AT3-1_no_pads
         COMMAND sh -c [=[! "$0" -i in.root]=] $<TARGET_FILE:channel-response>
         WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/s3_cli)
set_tests_properties(AT3-1_no_input AT3-1_no_pads PROPERTIES LABELS acceptance)

# A Stage 1 output has no "electrons" ntuple, so Stage 3 has to refuse it.
acceptance_run(AT3-1_stage1only_run s3_noelectrons drift_stage1.mac)
add_test(NAME AT3-1_no_electrons
         COMMAND sh -c
                 [=[! "$0" -i Ar_200mbar_5.5MeV.root -p "$1" && test -z "$(ls *_readout.root 2>/dev/null)"]=]
                 $<TARGET_FILE:channel-response> ${PADS_CSV}
         WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/s3_noelectrons)
set_tests_properties(AT3-1_no_electrons PROPERTIES LABELS acceptance
                     DEPENDS AT3-1_stage1only_run)

# --- AT3-3 end to end: Stage 1 -> Stage 2 -> Stage 3 ------------------------
# He at 200 mbar, 0.3 MeV. Its Magboltz table (100 V/cm) is a new one, so this
# run has to wait for AT2-4, which empties the shared cache directory.
acceptance_run(AT3-3_stage1 s3_end2end at3_stage1.mac)

add_test(NAME AT3-3_stage2
         COMMAND $<TARGET_FILE:drift-electrons>
                 -i He_200mbar_0.3MeV.root -v 2000 -f 0.1 -c ${S2_CACHE}
         WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/s3_end2end)
set_tests_properties(AT3-3_stage2 PROPERTIES LABELS acceptance
                     DEPENDS "AT3-3_stage1;AT2-4_cache" TIMEOUT 900)

add_test(NAME AT3-3_run
         COMMAND $<TARGET_FILE:channel-response>
                 -i He_200mbar_0.3MeV_2000V.root -p ${PADS_CSV} -w 0
         WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/s3_end2end)
set_tests_properties(AT3-3_run PROPERTIES LABELS acceptance
                     DEPENDS AT3-3_stage2)

# The alpha stops about 74 mm into the gas, so the pads that see electrons lie
# between the entrance plane (z = -250 mm) and the end of the track. The range
# straggles (the ten alphas stop between z = -183 and z = -167 mm), and on top
# of that comes three sigma of transverse diffusion (4 mm): -167 + 12 = -155.
set(AT3_3_CALL "check_readout.C(\"He_200mbar_0.3MeV_2000V_readout.root\",\"He_200mbar_0.3MeV_2000V.root\",\"${PADS_CSV}\",\"He\",200,2000,10,0,0.9,0.99,0.99,-250,-155)")
acceptance_check(AT3-3_check s3_end2end AT3-3_run "${AT3_3_CALL}")

add_test(NAME AT3-1_output_name
         COMMAND sh -c [=[test -f He_200mbar_0.3MeV_2000V_readout.root]=]
         WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/s3_end2end)
set_tests_properties(AT3-1_output_name PROPERTIES LABELS acceptance
                     DEPENDS AT3-3_run)

# --- AT3-4 a window that opens after the last electron ----------------------
# The output name is the one of AT3-3, so this runs on a copy of the Stage 2
# file in its own directory and the two stay independent.
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/s3_window)
add_test(NAME AT3-4_run
         COMMAND sh -c [=[
set -e
cp ../s3_end2end/He_200mbar_0.3MeV_2000V.root .
"$0" -i He_200mbar_0.3MeV_2000V.root -p "$1" -w 30
]=]
                 $<TARGET_FILE:channel-response> ${PADS_CSV}
         WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/s3_window)
set_tests_properties(AT3-4_run PROPERTIES LABELS acceptance
                     DEPENDS AT3-3_stage2)
acceptance_check(AT3-4_check s3_window AT3-4_run
                 [[check_readout_empty.C("He_200mbar_0.3MeV_2000V_readout.root",10,30)]])

# ============================================================================
# Stage 4: aget-shaper (TODO/07)
# ============================================================================

set(TPCDAQ_MACROS ${PROJECT_SOURCE_DIR}/external/tpcdaq-macros)
set(MAKE_CHANNEL_MAP ${PROJECT_SOURCE_DIR}/scripts/make_channel_map.py)

# --- AT4-3 end to end, on the Stage 3 file of AT3-3 -------------------------
# -g is pinned here (and in AT4-4 below): these tests check the shaper itself
# at a known gain, not the calibrated default, which TODO/08 moved to 3600.
add_test(NAME AT4-3_run
         COMMAND $<TARGET_FILE:aget-shaper>
                 -i He_200mbar_0.3MeV_2000V_readout.root -g 1000
         WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/s3_end2end)
set_tests_properties(AT4-3_run PROPERTIES LABELS acceptance DEPENDS AT3-3_run)

# The settings of TODO/07: 223 ns peaking, 120 fC range, gain 1000 (pinned on
# the command line above), pedestal 450, noise sigma 6. A pulse of more than 100 counts has to be there, and
# every pulse whose area is above 3000 counts (where the noise of the summed
# cells is far below 5 %) has to carry the charge of Stage 3.
set(AT4_3_CALL "check_raw.C(\"He_200mbar_0.3MeV_2000V_raw.root\",\"He_200mbar_0.3MeV_2000V_readout.root\",10,450,6,1000,120,223,100,3000)")
acceptance_check(AT4-3_check s3_end2end AT4-3_run "${AT4_3_CALL}")

add_test(NAME AT4-1_output_name
         COMMAND sh -c [=[test -f He_200mbar_0.3MeV_2000V_raw.root]=]
         WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/s3_end2end)
set_tests_properties(AT4-1_output_name PROPERTIES LABELS acceptance
                     DEPENDS AT4-3_run)

# An event whose time window caught no electron is written all the same, with
# pedestal and noise only: AT3-4 left ten of them behind in s3_window.
add_test(NAME AT4-1_empty_run
         COMMAND $<TARGET_FILE:aget-shaper>
                 -i He_200mbar_0.3MeV_2000V_readout.root
         WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/s3_window)
set_tests_properties(AT4-1_empty_run PROPERTIES LABELS acceptance
                     DEPENDS AT3-4_run)
acceptance_check(AT4-1_empty_check s3_window AT4-1_empty_run
                 [[check_raw_empty.C("He_200mbar_0.3MeV_2000V_raw.root",10,450,6,100)]])

# --- AT4-4 the closed loop through the macros of the real detector ----------
# CO2 at 100 mbar and 1200 V are the conditions analyzeUVW.C assumes. The
# Magboltz table for 60 V/cm is a new one, so this waits for AT2-4, which
# empties the shared cache directory.
acceptance_run(AT4-4_stage1 s4_tracks at4_stage1.mac)

add_test(NAME AT4-4_stage2
         COMMAND $<TARGET_FILE:drift-electrons>
                 -i CO2_100mbar_2MeV.root -v 1200 -f 0.1 -c ${S2_CACHE}
         WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/s4_tracks)
set_tests_properties(AT4-4_stage2 PROPERTIES LABELS acceptance
                     DEPENDS "AT4-4_stage1;AT2-4_cache" TIMEOUT 900)

# The electrons arrive 23 to 31 us after the alpha, so the 20.48 us window
# opens at 20 us.
add_test(NAME AT4-4_stage3
         COMMAND $<TARGET_FILE:channel-response>
                 -i CO2_100mbar_2MeV_1200V.root -p ${PADS_CSV} -w 20
         WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/s4_tracks)
set_tests_properties(AT4-4_stage3 PROPERTIES LABELS acceptance
                     DEPENDS AT4-4_stage2)

add_test(NAME AT4-4_run
         COMMAND $<TARGET_FILE:aget-shaper>
                 -i CO2_100mbar_2MeV_1200V_readout.root -g 1000
         WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/s4_tracks)
set_tests_properties(AT4-4_run PROPERTIES LABELS acceptance
                     DEPENDS AT4-4_stage3)

# analyzeUVW.C reads "channel_map.csv" from its working directory.
add_test(NAME AT4-4_channel_map
         COMMAND ${PYTHON3_EXECUTABLE} ${MAKE_CHANNEL_MAP} ${PADS_CSV}
                 channel_map.csv
         WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/s4_tracks)
set_tests_properties(AT4-4_channel_map PROPERTIES LABELS acceptance)

add_test(NAME AT4-4_uvw
         COMMAND ${ROOT_EXECUTABLE} -l -b -q
                 "${TPCDAQ_MACROS}/analyzeUVW.C(\"CO2_100mbar_2MeV_1200V_raw.root\")"
         WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/s4_tracks)
set_tests_properties(AT4-4_uvw PROPERTIES LABELS acceptance
                     DEPENDS "AT4-4_run;AT4-4_channel_map")

add_test(NAME AT4-4_tracks
         COMMAND ${ROOT_EXECUTABLE} -l -b -q
                 "${TPCDAQ_MACROS}/makeTracks.C(\"CO2_100mbar_2MeV_1200V_uvw.root\")"
         WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/s4_tracks)
set_tests_properties(AT4-4_tracks PROPERTIES LABELS acceptance
                     DEPENDS AT4-4_uvw)

# The alpha of at4_stage1.mac has half of its direction along the drift axis.
# On the length see check_tracks.C: makeTracks.C measures the 2 %/98 % charge
# quantiles of 3 mm slice centroids, which is 63 mm for these 72.8 mm alphas.
acceptance_check(AT4-4_check s4_tracks AT4-4_tracks
                 [[check_tracks.C("CO2_100mbar_2MeV_1200V_tracks.root",10,8,63.0,0.10,0.5,0.1)]])

endif()  # EXISTS ${PADS_CSV}
