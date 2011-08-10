var fs = require("fs");
var path = require("path");
var cl = require("./clucene").CLucene;

var lucene = new cl.Lucene();

if (!path.existsSync("tweets.lucene")) {
    var objects = fs.readFileSync("user_timeline.json", "utf8").split("\n");
    var counter = 0;
    var indexCounter = 0;
    function done() {
        console.log("Added " + counter + " entries in " + indexCounter/1000.0 + "s");
        doSearch();
    }
    function processNext() {
        var objText = objects.shift();
        if (objText == "") {
            return done();
        }
        try {
            var obj = JSON.parse(objText);
        } catch(E) {
            return;
        }
        var doc = new cl.Document();
        doc.addField("_id", obj.data.id.toString(), 65);
        doc.addField("content", obj.data.user.name + " - " + obj.data.user.screen_name + " - " + obj.data.created_at + " - " + obj.data.text, 33);
        lucene.addDocument(doc, "tweets.lucene", function(err, indexTime) {
            if (err) {
                console.log("Error adding: " + err);
            }
            indexCounter += indexTime;
            process.nextTick(processNext);
        });
        counter++;
    }
    processNext();
} else {
    doSearch();
}

function doSearch() {
    if (process.argv.length == 3) {
        console.log("Searching for " + process.argv[2]);
        lucene.search("tweets.lucene", "content:(" + process.argv[2] + ")", function(err, results) {
            if (err) {
                console.log("Search error: " + err);
                return;
            }
            var objects = fs.readFileSync("user_timeline.json", "utf8").split("\n");
            objects = objects.map(function(objText) { 
                try {
                    var json = JSON.parse(objText);
                    return json;
                } catch(E) {
                    return undefined;
                }
            }).filter(function(obj) { return obj != undefined; });
            results.forEach(function(result) {
                var id = parseInt(result.id);
                objects.some(function(obj) {
                    if (obj.data.id == id) {
                        console.log("score(" + result.score + ") " + obj.data.created_at + " - " + obj.data.text);
                        return true;
                    } else {
                        return false;
                    }

                });
            });
        });
    }
}
