'use strict';

const path = require('path');
const util = require('util');
const fs = require('fs');
const d3 = require('d3-queue');
const child_process = require('child_process');
const tryConnect = require('../lib/try_connect');

// Sets up all constants that are valid for all features
module.exports = function () {
    this.initializeEnv = (callback) => {
        this.TIMEOUT = process.env.CUCUMBER_TIMEOUT && parseInt(process.env.CUCUMBER_TIMEOUT) || 5000;
        // set cucumber default timeout
        this.setDefaultTimeout(this.TIMEOUT);
        this.ROOT_PATH = process.cwd();

        this.TEST_PATH = path.resolve(this.ROOT_PATH, 'test');
        this.CACHE_PATH = path.resolve(this.TEST_PATH, 'cache');
        this.LOGS_PATH = path.resolve(this.TEST_PATH, 'logs');

        this.PROFILES_PATH = path.resolve(this.ROOT_PATH, 'profiles');
        this.FIXTURES_PATH = path.resolve(this.ROOT_PATH, 'unit_tests/fixtures');
        this.BIN_PATH = process.env.OSRM_BUILD_DIR && process.env.OSRM_BUILD_DIR || path.resolve(this.ROOT_PATH, 'build');
        var stxxl_config = path.resolve(this.ROOT_PATH, 'test/.stxxl');
        if (!fs.existsSync(stxxl_config)) {
            return callback(new Error('*** '+stxxl_config+ 'does not exist'));
        }

        this.DEFAULT_ENVIRONMENT = Object.assign({STXXLCFG: stxxl_config}, process.env);
        this.DEFAULT_PROFILE = 'bicycle';
        this.DEFAULT_INPUT_FORMAT = 'osm';
        this.DEFAULT_LOAD_METHOD = 'datastore';
        this.DEFAULT_ORIGIN = [1,1];
        this.OSM_USER = 'osrm';
        this.OSM_GENERATOR = 'osrm-test';
        this.OSM_UID = 1;
        this.OSM_TIMESTAMP = '2000-01-01T00:00:00Z';
        this.WAY_SPACING = 100;
        this.DEFAULT_GRID_SIZE = 100; // meters

        this.OSRM_PORT = process.env.OSRM_PORT && parseInt(process.env.OSRM_PORT) || 5000;
        this.HOST = 'http://127.0.0.1:' + this.OSRM_PORT;

        // TODO make sure this works on win
        if (process.platform.match(/indows.*/)) {
            this.TERMSIGNAL = 9;
            this.EXE = '.exe';
            this.LIB = '.dll';
            this.QQ = '"';
        } else {
            this.TERMSIGNAL = 'SIGTERM';
            this.EXE = '';

            // heuristically detect .so/.a suffix
            this.LIB = null;

            try {
                const dot_a = util.format('%s/libosrm%s', this.BIN_PATH, '.a');
                fs.accessSync(dot_a, fs.F_OK);
                this.LIB = '.a';
            } catch(e) { /*nop*/ }

            try {
                const dot_so = util.format('%s/libosrm%s', this.BIN_PATH, '.so');
                fs.accessSync(dot_so, fs.F_OK);
                this.LIB = '.so';
            } catch(e) { /*nop*/ }

            if (!this.LIB) {
                throw new Error('*** Unable to detect dynamic or static libosrm libraries');
            }

            this.QQ = '';
        }

        this.OSRM_EXTRACT_PATH = path.resolve(util.format('%s/%s%s', this.BIN_PATH, 'osrm-extract', this.EXE));
        this.OSRM_CONTRACT_PATH = path.resolve(util.format('%s/%s%s', this.BIN_PATH, 'osrm-contract', this.EXE));
        this.OSRM_ROUTED_PATH = path.resolve(util.format('%s/%s%s', this.BIN_PATH, 'osrm-routed', this.EXE));
        this.LIB_OSRM_EXTRACT_PATH = util.format('%s/libosrm_extract%s', this.BIN_PATH, this.LIB),
        this.LIB_OSRM_CONTRACT_PATH = util.format('%s/libosrm_contract%s', this.BIN_PATH, this.LIB),
        this.LIB_OSRM_PATH = util.format('%s/libosrm%s', this.BIN_PATH, this.LIB);

        // eslint-disable-next-line no-console
        console.info(util.format('Node Version', process.version));
        if (parseInt(process.version.match(/v(\d)/)[1]) < 4) throw new Error('*** Please upgrade to Node 4.+ to run OSRM cucumber tests');

        fs.exists(this.TEST_PATH, (exists) => {
            if (exists)
                return callback();
            else
                return callback(new Error('*** Test folder doesn\'t exist.'));
        });
    };

    this.getProfilePath = (profile) => {
        return path.resolve(this.PROFILES_PATH, profile + '.lua');
    };

    this.verifyOSRMIsNotRunning = (callback) => {
        tryConnect(this.OSRM_PORT, (err) => {
            if (!err) return callback(new Error('*** osrm-routed is already running.'));
            else callback();
        });
    };

    this.verifyExistenceOfBinaries = (callback) => {
        var verify = (binPath, cb) => {
            fs.exists(binPath, (exists) => {
                if (!exists) return cb(new Error(util.format('%s is missing. Build failed?', binPath)));
                var helpPath = util.format('%s --help > /dev/null 2>&1', binPath);
                child_process.exec(helpPath, (err) => {
                    if (err) {
                        return cb(new Error(util.format('*** %s exited with code %d', helpPath, err.code)));
                    }
                    cb();
                });
            });
        };

        var q = d3.queue();
        [this.OSRM_EXTRACT_PATH, this.OSRM_CONTRACT_PATH, this.OSRM_ROUTED_PATH].forEach(bin => { q.defer(verify, bin); });
        q.awaitAll(callback);
    };

    process.on('exit', () => {
        this.osrmLoader.shutdown(() => {});
    });

    process.on('SIGINT', () => {
        process.exit(2);
        // TODO need to handle for windows??
    });
};
