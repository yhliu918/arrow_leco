# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.

#
# arrow_dataset_java
#

# Headers: top level

project(arrow_dataset_java)

# Find java/jni
include(FindJava)
include(UseJava)
include(FindJNI)

message("generating headers to ${JNI_HEADERS_DIR}")

add_jar(arrow_dataset_java
        src/main/java/org/apache/arrow/dataset/jni/JniLoader.java
        src/main/java/org/apache/arrow/dataset/jni/JniWrapper.java
        src/main/java/org/apache/arrow/dataset/file/JniWrapper.java
        src/main/java/org/apache/arrow/dataset/jni/NativeMemoryPool.java
        src/main/java/org/apache/arrow/dataset/jni/ReservationListener.java
        GENERATE_NATIVE_HEADERS
        arrow_dataset_java-native
        DESTINATION
        ${JNI_HEADERS_DIR})
