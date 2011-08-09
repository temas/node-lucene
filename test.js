var cl = require("./clucene").CLucene;

var lucene = new cl.Lucene();
var doc = new cl.Document();

try {
    doc.addField(1, "test", 65);
} catch (E) {
    console.log("Got " + E);
}
try {
    doc.addField("correct", 1, 65);
} catch (E) {
    console.log("Got " + E);
}
try {
    doc.addField("correct", "correct", "test");
} catch (E) {
    console.log("Got " + E);
}
doc.addField("_id", "1234", 65);
doc.addField("name", "Thomas Muldowney", 33);

lucene.addDocument(doc, "test.lc", function(err, indexTime) {
    console.log("Time to index: " + indexTime + " ms.");
    lucene.search("test.lc", "name:Muld*", function(err, results) {
        console.log("Got error: " + err);
        console.log("Results:");
        console.dir(results);
    });
});

