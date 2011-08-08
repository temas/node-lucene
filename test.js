var cl = require("./clucene").CLucene;

var lucene = new cl.Lucene;
var doc = new cl.Document;

doc.addField("_id", "1234", 65);
doc.addField("name", "Thomas Muldowney", 33);
lucene.addDocument(doc, "test.lc");
lucene.search("test.lc", "*:Muld*", function(err, results) {
    console.log("Got error: " + err);
    console.log("Results:");
    console.dir(results);
});

