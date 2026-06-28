module specify_complete;

  specify

    specparam tRise  = 1;
    specparam tFall  = 2;
    specparam tZrise = 3:4:5;
    specparam tA = 1, tB = 2:3:4;

    // --- path_delay_expression: all 5 count forms ---

    // 1-value: single constant_mintypmax_expression (applied to all transitions)
    (a => z1) = 10;

    // 1-value: mintypmax triple (min:typ:max) -- single path_delay_expression
    (a => z2) = 80:100:150;

    // 2-value: (trise, tfall)
    (a => z3) = (10, 20);

    // 2-value: mintypmax both slots
    (a => z4) = (80:100:150, 60:90:120);

    // 3-value: (trise, tfall, tz)
    (a => z5) = (10, 20, 30);

    // 3-value: mintypmax all three slots
    (a => z6) = (80:100:150, 60:90:120, 50:70:100);

    // 6-value: (t01, t10, t0z, tz1, t1z, tz0)
    (a => z7) = (1, 2, 3, 4, 5, 6);

    // 12-value: all transitions including X
    (a => z8) = (1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12);

    // 1-value using specparam reference
    (a => z9) = tRise;

    // 2-value using specparam references
    (a => z10) = (tRise, tFall);

    // 3-value using specparam references
    (a => z11) = (tRise, tFall, tZrise);

    pulsestyle_onevent  out1;
    pulsestyle_onevent  vout[0];
    pulsestyle_ondetect out2;
    pulsestyle_ondetect vout2[3:0];
    showcancelled    out12;
    noshowcancelled  out13;
    noshowcancelled  vout13[0];

    // specparams for timing check limits
    specparam tSetup = 1, tHold = 2;
    specparam tRecovery = 3, tRemoval = 4;
    specparam tSkew = 5, tTimeskew = 6;
    specparam tFull1 = 7, tFull2 = 8;
    specparam tSu = 9, tHold2 = 10;
    specparam tRec = 11, tRem = 12;
    specparam tPeriod = 20, tWidth = 21;
    specparam tNoChgStart = 0, tNoChgEnd = 1;

    // --- system_timing_check: all 12 forms ---

    // $setup: data_event first, reference_event second
    // -> tchkDataTerm=in1, tchkRefTerm=posedge clk, delay=tSetup
    $setup(in1, posedge clk, tSetup);

    // $hold: reference_event first, data_event second
    // -> tchkRefTerm=posedge clk, tchkDataTerm=in1, delay=tHold
    $hold(posedge clk, in1, tHold);
    $hold(negedge clk, in1, tHold);
    $hold(posedge clk, posedge in1, tHold);

    // $recovery: reference_event first, data_event second
    $recovery(posedge clk, in1, tRecovery);

    // $removal: reference_event first, data_event second
    $removal(posedge clk, in1, tRemoval);

    // $skew: reference_event first, data_event second
    $skew(posedge clk, in1, tSkew);

    // $timeskew: reference_event first, data_event second
    $timeskew(posedge clk, in1, tTimeskew);

    // $fullskew: reference_event, data_event, setup_limit, hold_limit
    // -> 2nd limit parsed into paNotifier by grammar; code routes it to delays
    $fullskew(posedge clk, in1, tFull1, tFull2);

    // $setuphold: reference_event, data_event, setup_limit, hold_limit
    // -> paDollar_setuphold_timing_check branch (two timing_check_limits)
    $setuphold(posedge clk, in1, tSu, tHold2);

    // $recrem: reference_event, data_event, recovery_limit, removal_limit
    // -> paDollar_setuphold_timing_check branch (same structure as $setuphold)
    $recrem(posedge clk, in1, tRec, tRem);

    // $period: controlled_timing_check_event, limit
    // -> paDollar_period_timing_check branch; tchkRefTerm=posedge clk
    $period(posedge clk, tPeriod);

    // $width: controlled event, limit, threshold
    // -> paDollar_width_timing_check branch; threshold not yet captured
    $width(posedge clk, tWidth, 0);

    // $nochange: reference_event, data_event, start_edge_offset, end_edge_offset
    // -> paDollar_setup_timing_check branch; offsets map to limit+notifier slots
    $nochange(posedge clk, in1, tNoChgStart, tNoChgEnd);

    // --- edge cases ---

    // With notifier variable (4th arg -> tchkNotifier)
    $setup(in1, posedge clk, tSetup, tc_notifier);

    // With timing check condition (&&& expr on data_event)
    $setup(in1 &&& cond, posedge clk, tSetup);

    // With timing check condition on both events
    $hold(posedge clk &&& cond, in1 &&& cond, tHold);

    // With mintypmax timing check limit (min:typ:max)
    $setup(in1, posedge clk, 1:2:3);

    // $setuphold with notifier (5th arg)
    $setuphold(posedge clk, in1, tSu, tHold2, tc_notifier);

    // $nochange with real notifier (5-arg form -> dollar_nochange_timing_check)
    $nochange(posedge clk, in1, tNoChgStart, tNoChgEnd, tc_notifier);

    // $timeskew with event_based_flag (5-arg -> dollar_timeskew_timing_check)
    $timeskew(posedge clk, in1, tTimeskew, tc_notifier, 1);

    // $fullskew with notifier and event_based_flag (5-arg -> dollar_fullskew_timing_check)
    $fullskew(posedge clk, in1, tFull1, tFull2, tc_notifier, 1);

  endspecify

endmodule