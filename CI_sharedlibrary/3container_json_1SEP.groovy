pipeline {
    agent none
    stages {
        stage('Container A') {
            agent {
               docker { image 'test123:latest' }
            }
            steps {
                echo 'Create superman map and writeJSON in container A'
                script {
                //create map and writeJSON
                def supermanmap = [:]
                supermanmap.firstname = "Clark"
                echo 'first name is ' + supermanmap
                supermanmap.lastname = "Kent"
                echo ' ' + supermanmap
                supermanmap.gender = "Male"
                echo ' ' + supermanmap
                supermanmap.age = "56?"
                echo ' ' + supermanmap
                supermanmap.address = "1234 Superman ln, New York, NY, 98345"
                echo ' ' + supermanmap
                supermanmap.powers = ["eye lasers", "x-ray vision", "superhuman strength"]
                echo ' ' + supermanmap
                supermanmap.phonenumber = "1234567899"
                echo ' ' + supermanmap
                echo 'Supermanmap info: ' + supermanmap
                //use writeJSON to covert supermanmap to .json format
                echo 'writing supermanmap to .json file supermandata.json'
                writeJSON file: 'supermandata.json', json: supermanmap
                    def read = readJSON file: 'supermandata.json'
                    sh 'cat supermandata.json'
                    sh 'pwd'
                }//end script 1
            }//end steps 1
        }//end stage 1 Container A   
        stage('Container B') {
            agent {
                docker {image 'ubuntu:latest'}
            }
            steps {
            echo 'Create Container B, read supermandata.json and add weaknesses data'
                script {
                //read supermantdata.json and add weaknesses data
                def supermanjson = readJSON file: '/var/jenkins_home/workspace/json_a_b_c_01_SEP_22/supermandata.json'
                echo 'Reading superman.json from container A and def as supermanjson map: ' + supermanjson
                echo 'adding weakness to supermandata.json by editing supermanjson map and writing to supermandata.json'
                supermanjson.weaknesses = "Kryptonite, Lois Lane, Sitcoms"
                writeJSON file: 'supermandata.json', json: supermanjson
                }//end script2
            }//end steps2
        }//end Container B stage
        stage('Container C') {
            agent {
                docker {image 'centos:latest'}
            }
            steps {
            echo 'Create Container C, read supermandata.json and show weaknesses data was added'
                script {
                //read supermantdata.json and show weaknesses data was added in container B
                def supermanjson = readJSON file: '/var/jenkins_home/workspace/json_a_b_c_01_SEP_22/supermandata.json'
                echo 'Reading supermanjson map from container B: ' + supermanjson
                //read appended supermandata.json file
                echo 'Reading supermandata.json file appended from Container B'
                sh 'cat /var/jenkins_home/workspace/json_a_b_c_01_SEP_22/supermandata.json'
                }//end script3
            }//end steps3
        }//end Container C stage
    }//end stages
}//end pipeline