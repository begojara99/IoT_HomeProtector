# IoT_HomeProtector


##### esp32 cam 모듈과 esp32 에 따로 /Camera/esp32_cam.ino , /HomeProtector/HomeProtector.ino 업로드 후 결합


##### aws 람다함수
```
const AWS = require('aws-sdk');

var s3 = new AWS.S3();

exports.handler = (event, context, callback) => {
  // Extracting data from the event
  let encodedImage = event.base64Image;
  let Folder = event.S3Folder;
  let Filename = event.Filename;
  // Decoding the base64-encoded image
  let decodedImage = Buffer.from(encodedImage, 'base64');
  // Setting up S3 parameters
  var filePath = "images/" + Folder + "/" + Filename + ".jpg";
  var params = {
    "Body": decodedImage,
    "Bucket": "protector-camera-bucket",
    "Key": filePath  
  };
  // Uploading to S3
  s3.upload(params, function(err, data) {
    if(err) {
      callback(err, null);
    } else {
      let response = {
        "statusCode": 200,
        "headers": {
          "my_header": "my_value"
        },
        "body": JSON.stringify(data),
        "isBase64Encoded": false
      };
      callback(null, response);
    }
  });
};
```

##### 매칭 템플릿
```
{
    "S3Folder" : "$input.params('folder')",
    "Filename" : "$input.params('object')",
    "base64Image" : "$input.body"
}
```
