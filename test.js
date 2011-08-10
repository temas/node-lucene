var assert = require("assert");
var cl     = require("./clucene").CLucene;

var lucene = new cl.Lucene();
var doc    = new cl.Document();

lucene.addDocument(doc, "/private/var/virusmails/test.lc", function(err, indexTime) {
    assert.ok(err != null);
});

var hadError = false;
try {
    doc.addField(1, "test", 65);
} catch (E) {
    hadError = true;
}
assert.ok(hadError);
hadError = false;
try {
    doc.addField("correct", 1, 65);
} catch (E) {
    hadError = true;
}
assert.ok(hadError);
hadError = false;
try {
    doc.addField("correct", "correct", "test");
} catch (E) {
    hadError = true;
}
assert.ok(hadError);
hadError = false;
try {
    doc.addField("id", "", 100);
} catch(E) {
    hadError = true;
}
assert.ok(hadError);
hadError = false;
doc.addField("_id", "1234", 65);
doc.addField("name", "Thomas Muldowney", 33);

lucene.addDocument(doc, "test.lc", function(err, indexTime) {
    console.log("Time to index: " + indexTime + " ms.");
    lucene.search("bad.lc", "*:*", function(err, results) {
        assert.equal(results, null);
        lucene.search("test.lc", "bad:bad:bad", function(err, results) {
            assert.equal(results, null);
            lucene.search("test.lc", "name:Muld*", function(err, results) {
                assert.ifError(err);
                console.log("Results:");
                console.dir(results);
            });
        });
    });
});

