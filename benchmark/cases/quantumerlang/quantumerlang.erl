-module(quantumerlang).
-export([main/0]).

-define(WORKERS, 64).
-define(ROUNDS, 300000).
-define(MOD, 1000000007).
-define(EXPECTED_CHECKSUM, 272862553).

bias(Lane) ->
    ((Lane * 7) + 3) rem 97 + 1.

phase(Lane) ->
    ((Lane * 11) + 5) rem 89 + 1.

salt(Lane) ->
    ((Lane * 13) + 17) rem 101 + 1.

alive(Lane) ->
    (Lane rem 3) =/= 1.

quantum_flux(Value) ->
    ((Value * 31) + 7) rem ?MOD.

quantum_reply(Request, Bias, Phase, Salt, true, Lane) ->
    quantum_flux(((Request * 17) + Bias + Phase + Salt + Lane) rem ?MOD);
quantum_reply(Request, Bias, Phase, Salt, false, Lane) ->
    quantum_flux(((Request * 17) + Bias + Salt + Lane + ?MOD - Phase) rem ?MOD).

worker(Lane, Bias, Phase, Salt, Alive, Cell) ->
    receive
        {fold, From, Index} ->
            Request = ((Index * 13) + Cell + Lane) rem ?MOD,
            Reply = quantum_reply(Request, Bias, Phase, Salt, Alive, Lane),
            NextCell = (Reply + Cell + Index + Lane) rem ?MOD,
            From ! {reply, Reply, NextCell},
            worker(Lane, Bias, Phase, Salt, Alive, NextCell);
        {stop, From} ->
            From ! {stopped, Lane, Cell},
            ok
    end.

make_workers(Lane, Acc) when Lane >= ?WORKERS ->
    list_to_tuple(lists:reverse(Acc));
make_workers(Lane, Acc) ->
    Worker = spawn(fun() ->
        worker(Lane, bias(Lane), phase(Lane), salt(Lane), alive(Lane), 0)
    end),
    make_workers(Lane + 1, [Worker | Acc]).

loop(_Workers, Index, Checksum) when Index >= ?ROUNDS ->
    Checksum;
loop(Workers, Index, Checksum) ->
    Lane = Index rem ?WORKERS,
    Worker = element(Lane + 1, Workers),
    Worker ! {fold, self(), Index},
    receive
        {reply, Reply, NextCell} ->
            NextChecksum = (Checksum + NextCell + Reply + Lane) rem ?MOD,
            loop(Workers, Index + 1, NextChecksum)
    end.

collect_cells(_Workers, Lane, Acc) when Lane >= ?WORKERS ->
    Acc rem ?MOD;
collect_cells(Workers, Lane, Acc) ->
    Worker = element(Lane + 1, Workers),
    Worker ! {stop, self()},
    receive
        {stopped, Lane, Cell} ->
            collect_cells(Workers, Lane + 1, (Acc + Cell) rem ?MOD)
    end.

main() ->
    Workers = make_workers(0, []),
    Checksum = loop(Workers, 0, 0),
    Observed = collect_cells(Workers, 0, 0),
    FinalScore = (Checksum + Observed) rem ?MOD,
    case FinalScore =:= ?EXPECTED_CHECKSUM of
        true -> halt(0);
        false -> halt(1)
    end.
