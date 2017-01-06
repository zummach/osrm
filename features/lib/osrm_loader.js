'use strict';

const fs = require('fs');
const util = require('util');
const Timeout = require('node-timeout');
const tryConnect = require('../lib/try_connect');
const errorReason = require('./utils').errorReason;

class OSRMBaseLoader{
    constructor (scope) {
        this.scope = scope;
        this.child = null;
    }

    launch (callback) {
        var limit = Timeout(this.scope.TIMEOUT, { err: new Error('*** Launching osrm-routed timed out.') });

        var runLaunch = (cb) => {
            this.osrmUp(() => { this.waitForConnection(cb); });
        };

        runLaunch(limit((e) => { if (e) callback(e); else callback(); }));
    }

    shutdown (callback) {
        if (!this.osrmIsRunning()) return callback();

        var limit = Timeout(this.scope.TIMEOUT, { err: new Error('*** Shutting down osrm-routed timed out.')});

        this.osrmDown(limit(callback));
    }

    osrmIsRunning () {
        return this.child && !this.child.killed;
    }

    osrmDown (callback) {
        if (this.osrmIsRunning()) {
            this.child.on('exit', (code, signal) => {callback();});
            this.child.kill();
        } else callback();
    }

    waitForConnection (callback) {
        var retryCount = 0;
        let retry = (err) => {
          if (err) {
            if (retryCount < 10) {
              retryCount++;
              setTimeout(() => { tryConnect(this.scope.OSRM_PORT, retry); }, 10);
            } else {
              callback(new Error("Could not connect to osrm-routed after ten retries."));
            }
          }
          else
          {
            callback();
          }
        };

        tryConnect(this.scope.OSRM_PORT, retry);
    }
};

class OSRMDirectLoader extends OSRMBaseLoader {
    constructor (scope) {
        super(scope);
    }

    load (inputFile, callback) {
        this.inputFile = inputFile;
        this.shutdown(() => {
            this.launch(callback);
        });
    }

    osrmUp (callback) {
        if (this.osrmIsRunning()) return callback(new Error("osrm-routed already running!"));

        this.child = this.scope.runBin('osrm-routed', util.format("%s -p %d", this.inputFile, this.scope.OSRM_PORT), this.scope.environment, (err) => {
          if (err) {
              throw new Error(util.format('osrm-routed %s: %s', errorReason(err), err.cmd));
          }
        });
        callback();
    }
};

class OSRMDatastoreLoader extends OSRMBaseLoader {
    constructor (scope) {
        super(scope);
    }

    load (inputFile, callback) {
        this.inputFile = inputFile;

        this.loadData((err) => {
            if (err) return callback(err);
            if (!this.osrmIsRunning()) this.launch(callback);
            else {
                this.scope.setupOutputLog(this.child, fs.createWriteStream(this.scope.scenarioLogFile, {'flags': 'a'}));
                callback();
            }
        });
    }

    loadData (callback) {
        this.scope.runBin('osrm-datastore', this.inputFile, this.scope.environment, (err) => {
            if (err) return callback(new Error('*** osrm-datastore exited with ' + err.code + ': ' + err));
            callback();
        });
    }

    osrmUp (callback) {
        if (this.osrmIsRunning()) return callback();

        this.child = this.scope.runBin('osrm-routed', util.format('--shared-memory=1 -p %d', this.scope.OSRM_PORT), this.scope.environment, (err) => {
            if (err) {
                throw new Error(util.format('osrm-routed %s: %s', errorReason(err), err.cmd));
            }
        });

        // we call the callback here, becuase we don't want to wait for the child process to finish
        callback();
    }
};

class OSRMLoader {
    constructor (scope) {
        this.scope = scope;
        this.sharedLoader = new OSRMDatastoreLoader(this.scope);
        this.directLoader = new OSRMDirectLoader(this.scope);
        this.method = scope.DEFAULT_LOAD_METHOD;
    }

    load (inputFile, callback) {
        if (this.method === 'datastore') {
            this.directLoader.shutdown((err) => {
              if (err) return callback(err);
              this.loader = this.sharedLoader;
              this.sharedLoader.load(inputFile, callback);
            });
        } else if (this.method === 'directly') {
            this.sharedLoader.shutdown((err) => {
              if (err) return callback(err);
              this.loader = this.directLoader;
              this.directLoader.load(inputFile, callback);
            });
        } else {
            callback(new Error('*** Unknown load method ' + method));
        }
    }

    setLoadMethod (method) {
        this.method = method;
    }

    shutdown (callback) {
        if (!this.loader) return callback();

        this.loader.shutdown(callback);
    }

    up () {
        return this.loader ? this.loader.osrmIsRunning() : false;
    }
};

module.exports = OSRMLoader;
