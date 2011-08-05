var cl = require("./clucene").CLucene;

var lucene = new cl.Lucene;
var doc = new cl.Document;

console.log("Created the lucene and doc, now adding the field.");
doc.addField("Key", "Value", 33);
console.log("Field added");
lucene.addDocument(doc, "test.lc");
console.log("Added the document");
console.dir(lucene.search("test.lc", "*:*"));

