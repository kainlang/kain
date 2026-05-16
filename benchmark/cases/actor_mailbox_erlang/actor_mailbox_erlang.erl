-module(actor_mailbox_erlang).
-export([main/0]).

-define(WORKERS, 4).
-define(ROUNDS, 200000).
-define(CHECKSUM_MOD, 1000000007).
-define(EXPECTED_CHECKSUM, 10399419).

worker(Bias) ->
    receive
        {call, From, Value} ->
            From ! {reply, Value + Bias},
            worker(Bias);
        stop ->
            ok
    end.

ask_worker(Workers, Lane, Request) ->
    Worker = lists:nth(Lane + 1, Workers),
    Worker ! {call, self(), Request},
    receive
        {reply, Reply} ->
            Reply
    end.

loop(_Workers, Index, Checksum) when Index >= ?ROUNDS ->
    Checksum;
loop(Workers, Index, Checksum) ->
    Lane = Index rem ?WORKERS,
    Request = Index rem 97,
    Reply = ask_worker(Workers, Lane, Request),
    NextChecksum = (Checksum + Reply + Lane) rem ?CHECKSUM_MOD,
    loop(Workers, Index + 1, NextChecksum).

stop_workers([]) ->
    ok;
stop_workers([Worker | Rest]) ->
    Worker ! stop,
    stop_workers(Rest).

main() ->
    Workers = [
        spawn(fun() -> worker(1) end),
        spawn(fun() -> worker(2) end),
        spawn(fun() -> worker(3) end),
        spawn(fun() -> worker(4) end)
    ],
    _Warm0 = ask_worker(Workers, 0, 0),
    _Warm1 = ask_worker(Workers, 1, 0),
    _Warm2 = ask_worker(Workers, 2, 0),
    _Warm3 = ask_worker(Workers, 3, 0),
    Checksum = loop(Workers, 0, 0),
    stop_workers(Workers),
    case Checksum =:= ?EXPECTED_CHECKSUM of
        true -> halt(0);
        false -> halt(1)
    end.
