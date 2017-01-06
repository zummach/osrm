module.exports = {
    default: '--strict --tags ~@stress --tags ~@todo --require features/support --require features/step_definitions',
    verify: '--strict --tags ~@stress --tags ~@todo -f progress --require features/support --require features/step_definitions',
    todo: '--strict --tags @todo --require features/support --require features/step_definitions',
    all: '--strict --require features/support --require features/step_definitions'
}



