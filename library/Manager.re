type op;
let toOp: 'a => op = Obj.magic;
type undoOperation = (Util.id, op);
type apply =
  (~handleUndo: op => unit, list(op)) =>
  result(unit, list(Util.operationError(op)));

let applyFns: ref(list((Util.id, apply))) = ref([]);
let register = (id, apply) => {
  applyFns := [(id, apply |> Obj.magic), ...applyFns^];
};

let mapError = fn =>
  fun
  | Ok(_) as a => a
  | Error(err) => fn(err);

let baseApply = (~handleUndo, ops: list(undoOperation)) => {
  applyFns^
  |> Tablecloth.List.map(~f=((id, fn)) => {
       ops
       |> Tablecloth.List.filterMap(~f=((opId, op)) =>
            id === opId ? Some(op) : None
          )
       |> fn(~handleUndo=op => handleUndo((id, op)))
       |> mapError(errs =>
            Error(errs |> Tablecloth.List.map(~f=err => (id, err)))
          )
     })
  |> Tablecloth.List.fold_left(~initial=Ok(), ~f=(res, memo) => {
       switch (memo, res) {
       | (Ok (), Ok ()) => Ok()
       | (Error(_) as e, Ok ())
       | (Ok (), Error(_) as e) => e
       | (Error(memoErrs), Error(errs)) =>
         Error(Tablecloth.List.concat([memoErrs, errs]))
       }
     });
};

module Undo =
  UndoRedo.Make({
    type operation = op;
    let apply = baseApply;
  });

let apply = (id: Util.id, ops) => {
  Undo.apply(ops |> Tablecloth.List.map(~f=op => (id, op |> toOp)));
};

let transaction: ref(list(undoOperation)) = ref([]);
let transactionRollbacks: ref(list(unit => unit)) = ref([]);

let addToTransaction = (id, ops, rollback) => {
  transaction :=
    Tablecloth.List.concat([
      transaction^,
      ops |> Tablecloth.List.map(~f=op => (id, op |> toOp)),
    ]);
  transactionRollbacks := [rollback, ...transactionRollbacks^];
};

let commitTransaction = () => {
  switch (Undo.applyTransaction(transaction^)) {
  | Ok () =>
    transactionRollbacks := [];
    transaction := [];
    Ok();
  | Error(_) as e =>
    transactionRollbacks^
    |> Tablecloth.List.iter(~f=rollbackFn => rollbackFn());
    transactionRollbacks := [];
    transaction := [];
    e;
  };
};

let applyTransaction = (id: Util.id, ops) => {
  Undo.applyTransaction(
    ops |> Tablecloth.List.map(~f=op => (id, op |> toOp)),
  );
};

let undo = Undo.undo;
let getUndoHistory = Undo.getUndoHistory;
let redo = Undo.redo;
let getRedoHistory = Undo.getRedoHistory;
let __reset__ = Undo.__reset__;
